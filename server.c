//portions of the code is inspired by 241 lecture code
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "common.h"
#include "format.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include "vector.h"

//functions
void estab_connection(char* port);
void run_request(int client_fd);
void handle_sigint(int signal);
//globals
static int sock_fd;    //server socket
static char* dir_name;   //temp dir name
static vector* files_vector = NULL;
static vector* sizes_vector = NULL;

//main method
int main(int argc, char **argv) {
    signal(SIGINT,handle_sigint);
    //check usage
    if (argc != 2) {
        print_server_usage();
        exit(1);
    }
    //make file vectors
    files_vector = vector_create(string_copy_constructor, string_destructor, string_default_constructor);
    sizes_vector = vector_create(int_copy_constructor, int_destructor, int_default_constructor);
    //make file storage
    char template[] = "tmpdir.XXXXXX";
    dir_name = mkdtemp(template);
    print_temp_directory(dir_name);
    chdir(dir_name);
    //make connection
    estab_connection(argv[1]);
    //start connecting
    while(1) {
        printf("Waiting for connection...\n");
        int client_fd = accept(sock_fd, NULL, NULL);
        
        printf("Connection made: client_fd=%d\n", client_fd);

        //deal with request
        run_request(client_fd);
        //done with client
        shutdown(client_fd , SHUT_RDWR);
        close(client_fd);
    }

    //end and clean ups
    //unlink();  //->remove files
    rmdir(dir_name);    //remove directory
}


void estab_connection(char* port) {
    printf("port: %s\n", port);
    //socket
    //int s;
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int s = getaddrinfo(NULL, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(1);
    }
    // New: Reuse port using SO_REUSEADDR-
    int optval = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    //bind
    if ( bind(sock_fd, result->ai_addr, result->ai_addrlen) != 0 )
    {
        perror("bind()");
        exit(1);
    }
    //listen
    if ( listen(sock_fd, 10) != 0 )
    {
        perror("listen()");
        exit(1);
    }
}

void run_request(int client_fd) {
    //get first line
    char *line_buffer = NULL;
    size_t line_buffer_size = 0;
    ssize_t line_length;
    FILE* sock_file = fdopen(client_fd, "r+");
    line_length = getline(&line_buffer, &line_buffer_size, sock_file);
    //get VERB from first line
    printf("running request: %s\n", line_buffer);
    if (strncmp(line_buffer, "GET", 3) == 0) {
        printf("request is GET\n");
        char* file_name = calloc(1, (line_length));   //strlen("GET ")
        strncpy(file_name, line_buffer + 4, line_length - 4);
        if (file_name[strlen(file_name)-1] == '\n') {
            file_name[strlen(file_name)-1] = '\0';
        }
        printf("file_name: %s\n", file_name);
        //check if file exists
        int file_exists = 0;
        int file_index = 0;
        size_t i = 0;
        for (i = 0; i < vector_size(files_vector); i++) {
            char* curr_file_name = vector_get(files_vector, i);
            if (strcmp(file_name, curr_file_name) == 0) {
                file_exists = 1;
                file_index = i;
                break;
            }
        }
        //does exist
        if (!file_exists) {
            //response
            char* msg = "ERROR\n";
            write_all_to_socket(client_fd, msg, strlen(msg));
            write_all_to_socket(client_fd, err_no_such_file, strlen(err_no_such_file));
        } else {
            //write first line
            char* msg = "OK\n";
            write_all_to_socket(client_fd, msg, strlen(msg));
            //get file size
            int file_size = *(int*)vector_get(sizes_vector, file_index);
            printf("file_size: %d\n", file_size);
            //write size to client
            char* size_in_char = calloc(1, (sizeof(size_t)+1));
            sprintf(size_in_char, "%s", (char*)&file_size);
            write_all_to_socket(client_fd, size_in_char, sizeof(size_t));
            //open file
            printf("file_name: %s\n", file_name);
            FILE* local_file = fopen(file_name, "r");
            int local_file_fd = fileno(local_file);

            printf("position-check-2\n");
            //copy the file over
            char* buffer = calloc(file_size, 1);
            ssize_t byte_read = read_all_from_socket(local_file_fd, buffer, file_size);
            
            printf("buffer: %s\n", buffer);
            printf("byte_read: %ld\n", byte_read);
            
            ssize_t byte_wriitten = write_all_to_socket(client_fd, buffer, byte_read);
            printf("byte_wriitten: %ld\n", byte_wriitten);

            
            fclose(local_file);
        }

    } else if (strncmp(line_buffer, "PUT", 3) == 0) {
        printf("request is PUT\n");
        //change dir to temp folder
        int rs = chdir(dir_name);
        printf("rs: %d\n", rs);
        
        //get file name
        char* file_name = calloc(1, (line_length));   //strlen("PUT ")
        strncpy(file_name, line_buffer + 4, line_length - 4);
        if (file_name[strlen(file_name)-1] == '\n') {
            file_name[strlen(file_name)-1] = '\0';
        }
        printf("file_name: %s\n", file_name);
        //create file
        FILE* output_file = fopen(file_name, "w");
        int output_file_fd = fileno(output_file);
        
        //get expected size
        fseek(sock_file, line_length, 0);
        char* size_buffer = malloc(10);
        fread(size_buffer, 1, sizeof(size_t), sock_file);
        size_t file_expected_size = *(size_t*)size_buffer;
        printf("file_expected_size: %zu\n", file_expected_size);
        
        //set file seek position
        fseek(sock_file, sizeof(size_t), 0);
        size_t real_size = 0;
        //keep geting lines and write to the file
        while (1) {
            //get new line
            line_length = getline(&line_buffer, &line_buffer_size, sock_file);
            if (line_length > 0) {
                real_size += line_length;
            }
            printf("line_length: %zd\n", line_length);
            //see if have read all
            if (line_length < 0) {
                //end of read
                break;
            }
            printf("line_buffer: %s\n", line_buffer);
            //write
            write_all_to_socket(output_file_fd, line_buffer, line_length);
        }
        //close file
        fclose(output_file);
        int error_happened = 0;
        //compare real_size of expected size
        if (real_size < file_expected_size) {
            print_too_little_data();
            error_happened = 1;
            //on error, delete output file
            remove(file_name);
            char* msg = "ERROR\nBad file size\n";
            write_all_to_socket(client_fd, msg, strlen(msg));

        } else if (real_size > file_expected_size) {
            print_received_too_much_data();
            error_happened = 1;
            //on error, delete output file
            remove(file_name);
            char* msg = "ERROR\nBad file size\n";
            write_all_to_socket(client_fd, msg, strlen(msg));
        }
        //send response
        if (!error_happened) {
            write_all_to_socket(client_fd, "OK\n", 3);
            //add file to vector(global "file system") if does not already exist
            printf("file_name: %s\n", file_name);
            //check if filename already exists
            int file_exists = 0;
            size_t i = 0;
            for (i = 0; i < vector_size(files_vector); i++) {
                char* curr_file_name = vector_get(files_vector, i);
                if (strcmp(file_name, curr_file_name) == 0) {
                    file_exists = 1;
                    break;
                }
            }
            //push to vector
            if (!file_exists) {
                vector_push_back(files_vector, file_name);
                vector_push_back(sizes_vector, &file_expected_size);
            }
        }

    } else if (strncmp(line_buffer, "LIST", 4) == 0) {
        printf("request is LIST\n");
        //check malformated request
        if (line_length != 5) {
            write_all_to_socket(client_fd, err_bad_request, strlen(err_bad_request));
        }
        line_length = getline(&line_buffer, &line_buffer_size, sock_file);
        if (line_length >= 0) {
            write_all_to_socket(client_fd, err_bad_request, strlen(err_bad_request));
        }
        //start acting
        size_t file_count = vector_size(files_vector);
        printf("file_count: %zu\n", file_count);
        //write first line
        write_all_to_socket(client_fd, "OK\n", 3);
        //make string
        char* buffer = calloc(1024, 1);
        int bytes_so_far = 0;
        size_t i = 0;
        //put a place holder for [size]  //not using
        //add file names
        for (i = 0; i < file_count; i++) {
            char* file_name = vector_get(files_vector, i);
            bytes_so_far += sprintf(buffer + bytes_so_far, "%s\n", file_name);
            printf("file_name: %s\n", file_name);
            printf("bytes_so_far: %d\n", bytes_so_far);
        }
        //write size
        printf("bytes_so_far: %d\n", bytes_so_far);
        char* size_in_char = calloc(1, (sizeof(size_t)+1));
        sprintf(size_in_char, "%s", (char*)&bytes_so_far);
        write_all_to_socket(client_fd, size_in_char, sizeof(size_t));
        //write file names
        write_all_to_socket(client_fd, buffer, bytes_so_far);

        //free and clean up
        
    } else if (strncmp(line_buffer, "DELETE", 6) == 0) {
        printf("request is DELETE\n");
        char* file_name = calloc(1, (line_length));   //strlen("PUT ")
        strncpy(file_name, line_buffer + 7, line_length - 7);
        if (file_name[strlen(file_name)-1] == '\n') {
            file_name[strlen(file_name)-1] = '\0';
        }
        printf("file_name: %s\n", file_name);
        //check if file exists
        int file_exists = 0;
        int file_index = 0;
        size_t i = 0;
        for (i = 0; i < vector_size(files_vector); i++) {
            char* curr_file_name = vector_get(files_vector, i);
            if (strcmp(file_name, curr_file_name) == 0) {
                file_exists = 1;
                file_index = i;
                break;
            }
        }
        //not does not exist
        if (!file_exists) {
            //response
            char* msg = "ERROR\n";
            write_all_to_socket(client_fd, msg, strlen(msg));
            write_all_to_socket(client_fd, err_no_such_file, strlen(err_no_such_file));
        } else {
            //exists -> delete -> rm from vector -> response
            remove(file_name);
            vector_erase(files_vector, file_index);
            vector_erase(sizes_vector, file_index);
            char* msg = "OK\n";
            write_all_to_socket(client_fd, msg, strlen(msg));
        }
    } else {
        //misolania
        write_all_to_socket(client_fd, err_bad_request, strlen(err_bad_request));
    }
}


//end server and clean memory
void handle_sigint(int signal) {
    printf("shuting down server\n");
    shutdown(sock_fd, SHUT_RDWR);
    close(sock_fd);
    //exit
    exit(0);
}