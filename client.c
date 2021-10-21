/**
 * Nonstop Networking
 * CS 241 - Spring 2020
 */
 //some code inspired by previous mp and lab
#include "common.h"
#include "format.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>
#include <sys/stat.h>

char **parse_args(int argc, char **argv);
verb check_args(char **args);


int main(int argc, char **argv) {
    //variables
    ssize_t block_size = 256;
    //parse input    {host, port, method, remote, local, NULL}
    char** parsed_args = parse_args(argc, argv);
    printf("parsed_args[0]: %s\n", parsed_args[0]);
    printf("parsed_args[1]: %s\n", parsed_args[1]);
    printf("parsed_args[2]: %s\n", parsed_args[2]);
    printf("parsed_args[3]: %s\n", parsed_args[3]);
    printf("parsed_args[4]: %s\n", parsed_args[4]);
    //get opt     { GET, PUT, DELETE, LIST, V_UNKNOWN }
    verb opt = check_args(parsed_args);
    printf("opt: %u\n", opt);
    //if put -> check file exists
    if (opt == 1) {
        //check file exists
        if (access(parsed_args[4], F_OK) != -1) {
            //file exist -> open file
            input_file = fopen(parsed_args[4], "r");
        } else {
            //X exist -> print -> exit
            exit(1);
        }
    }
    printf("file is valid\n");
    //connect to server
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4   (AF_INET6 for IPv6 addresses)
    hints.ai_socktype = SOCK_STREAM; // TCP

    int s = getaddrinfo(parsed_args[0], parsed_args[1], &hints, &result);
    if (s != 0) {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror(s));
      exit (1);
    }
    int sock_fd = socket(hints.ai_family, hints.ai_socktype, 0);
    
    int ok = connect(sock_fd, result->ai_addr, result->ai_addrlen);
    if( ok == -1) {
        perror("connect");
        exit(1);
    }
    printf("Connection successful\n");
    //send request and data
    char* request = NULL;
    if (opt == 0) {
        //GET
        request = calloc(300, 1);
        sprintf(request, "GET %s\n", parsed_args[3]);   // command and server file name
        //write to server
        write_all_to_socket(sock_fd, request, strlen(request));
        free(request);

    } else if (opt == 1) {
        //PUT
        //write command
        request = calloc(300, 1);
        sprintf(request, "PUT %s\n", parsed_args[3]);   // command and server file name
        FILE* input_file = fopen(parsed_args[4], "r");
        printf("request 1: %s\n", request);
        //write to server
        write_all_to_socket(sock_fd, request, strlen(request));
        //get file size
        free(request);
        //request = calloc(128, 1);
        struct stat s;
        stat(parsed_args[4], &s);
        size_t file_size = s.st_size;
        printf("file_size: %zu\n", file_size);
        printf("file_size: %s\n", (char*) &file_size);
        //sprintf(request + strlen(request), "%s", (char*) &file_size);
        printf("request 2: %s\n", request);
        write_all_to_socket(sock_fd, (char*) &file_size, sizeof(size_t));
        write file content
        char* 
        while(1) {
            //read
            char* buffer = calloc(block_size, 1);
            ssize_t byte_read = read_all_from_socket(sock_fd, buffer, block_size);
            //write
            write_all_to_socket(1, buffer, byte_read);
            //check end
            if (byte_read != block_size) {
                break;
            }
        }
        int input_fd = fileno(input_file);
        char* buffer = calloc(file_size, 1);
        ssize_t byte_read = read_all_from_socket(input_fd, buffer, file_size);
        ssize_t byte_wriitten = write_all_to_socket(sock_fd, buffer, byte_read);
        printf("buffer: %s\n", buffer);
        printf("byte_read: %ld\n", byte_read);
        printf("byte_wriitten: %ld\n", byte_wriitten);

        fclose(input_file);
        free(buffer);
    } else if (opt == 2) {
        //DELETE
        request = calloc(300, 1);
        sprintf(request, "DELETE %s\n", parsed_args[3]);   // command and server file name
        //write to server
        write_all_to_socket(sock_fd, request, strlen(request));
        free(request);
    } else if (opt == 3) {
        //LIST
        request = calloc(300, 1);
        sprintf(request, "LIST\n");
        ssize_t bytes_written = write_all_to_socket(sock_fd, request, strlen(request));
        if (bytes_written == (ssize_t)strlen(request)) {
            printf("list request write successful\n");
        } else {
            free(request);
            exit(1);
        }
        free(request);
    }

    //close connection
    shutdown(sock_fd, SHUT_WR);
    //get message and print to stdout
    if (opt == 0) {
        //GET
        //use getline
        char *line_buffer = NULL;
        size_t line_buffer_size = 0;
        ssize_t line_length;
        FILE *sock_file = fdopen(sock_fd, "r");
        FILE *local_file = fopen(parsed_args[4], "w");
        size_t file_expected_size = 0;
        size_t file_real_size = 0;
        //get line
        line_length = getline(&line_buffer, &line_buffer_size, sock_file);

        //check if ok
        if (strncmp(line_buffer, "OK", 2) == 0) {
            //if successful
            //file_real_size += line_length;
            int line_count = 0;
            while (line_length >= 0) {
                printf("line_buffer: %s\n", line_buffer);
                printf("--------------------------%d\n", line_count);
                //if first line -> "ok" -> continue
                if (line_count == 0){
                    if (strncmp(line_buffer, "OK", 2) == 0) {
                        //read another line
                        line_length = getline(&line_buffer, &line_buffer_size, sock_file);
                        //file_real_size += line_length;
                        line_count++;
                        continue;
                    } else {
                        break;
                    }
                } 
                
                //second line -> extra size
                if (line_count == 1) {
                    printf("position check-0\n");
                    char* size_extracted = line_buffer + sizeof(size_t);
                    file_expected_size = *(size_t*)line_buffer;
                    printf("file_expected_size: %zu\n", file_expected_size);
                    //write
                    fwrite(size_extracted, line_length - sizeof(size_t), 1, local_file);
                    //read another line
                    file_real_size += line_length - sizeof(size_t);
                    line_length = getline(&line_buffer, &line_buffer_size, sock_file);
                    line_count++;
                    //printf("file_real_size: %zu\n", file_real_size);
                    
                    if (line_length > 0) {
                        file_real_size += line_length;
                    }
                    //printf("file_real_size: %zu\n", file_real_size);
                    continue;
                }
                //write
                fwrite(line_buffer, line_length, 1, local_file);
                //read another line
                line_length = getline(&line_buffer, &line_buffer_size, sock_file);
                printf("file_real_size: %zu\n", file_real_size);
                if (line_length > 0) {
                    file_real_size += line_length;
                }
                printf("file_real_size: %zu\n", file_real_size);
                line_count++;
            }
            
            // compare sizes
            printf("file_expected_size: %zu\n", file_expected_size);
            printf("file_real_size: %zu\n", file_real_size);
            if (file_real_size < file_expected_size) {
                print_too_little_data();
            } else if (file_real_size > file_expected_size) {
                print_received_too_much_data();
            }
        } else if (strncmp(line_buffer, "ERROR", 5) == 0) {
            //error -> print error message
            //get next line
            getline(&line_buffer, &line_buffer_size, sock_file);
            //print error
            print_error_message(line_buffer);

        } else {
            print_invalid_response();
        }
        fclose(sock_file);
        fclose(local_file);
        free(line_buffer);

    } else if (opt == 1 || opt == 2) {
        //PUT or DELETE
        char* buffer = calloc(block_size, 1);
        ssize_t byte_read = read_all_from_socket(sock_fd, buffer, block_size);
        printf("buffer: %s\n", buffer);
        //check ok or error
        if (strncmp(buffer, "OK", 2) == 0) {
            print_success();
        } else if (strncmp(buffer, "ERROR", 5) == 0) {
            printf("got error\n");
            //find '\n'
            char* temp = buffer;
            while(temp) {
                if (*temp == '\n') {
                    temp++;
                    break;
                }
                temp++;
            }
            //print error
            print_error_message(temp);
        } else {
            print_invalid_response();
        }
        free(buffer);


    } else if (opt == 3) {
        //case LIST command
        char* buffer = calloc(block_size, 1);
        ssize_t byte_read = read_all_from_socket(sock_fd, buffer, block_size);
        printf("buffer: %s\n", buffer);
        printf("byte_read: %ld\n", byte_read);
        //no need to check ok or not
        //find '\n'
        char* temp = buffer;
        int count = 0;
        while(temp) {
            if (*temp == '\n') {
                temp++;
                break;
            }
            temp++;
            count++;
        }
        //parse response
        printf("count: %zu\n", *(size_t*)temp);
        char* copy = strdup(temp + sizeof(size_t));
        char * line = strtok(copy, "\n");
        while(line) {
            printf("%s\n", line);
            line  = strtok(NULL, "\n");
        }
        free(copy);
        free(buffer);
        
    }
    //free parse args
    free(parsed_args);
    free(result);
    //return
    return 0; 
}

/**
 * Given commandline argc and argv, parses argv.
 *
 * argc argc from main()
 * argv argv from main()
 *
 * Returns char* array in form of {host, port, method, remote, local, NULL}
 * where `method` is ALL CAPS
 */
char **parse_args(int argc, char **argv) {
    if (argc < 3) {
        return NULL;
    }

    char *host = strtok(argv[1], ":");
    char *port = strtok(NULL, ":");
    if (port == NULL) {
        return NULL;
    }

    char **args = calloc(1, 6 * sizeof(char *));
    args[0] = host;
    args[1] = port;
    args[2] = argv[2];
    char *temp = args[2];
    while (*temp) {
        *temp = toupper((unsigned char)*temp);
        temp++;
    }
    if (argc > 3) {
        args[3] = argv[3];
    }
    if (argc > 4) {
        args[4] = argv[4];
    }

    return args;
}

/**
 * Validates args to program.  If `args` are not valid, help information for the
 * program is printed.
 *
 * args     arguments to parse
 *
 * Returns a verb which corresponds to the request method
 */
verb check_args(char **args) {
    if (args == NULL) {
        print_client_usage();
        exit(1);
    }

    char *command = args[2];

    if (strcmp(command, "LIST") == 0) {
        return LIST;
    }

    if (strcmp(command, "GET") == 0) {
        if (args[3] != NULL && args[4] != NULL) {
            return GET;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "DELETE") == 0) {
        if (args[3] != NULL) {
            return DELETE;
        }
        print_client_help();
        exit(1);
    }

    if (strcmp(command, "PUT") == 0) {
        if (args[3] == NULL || args[4] == NULL) {
            print_client_help();
            exit(1);
        }
        return PUT;
    }

    // Not a valid Method
    print_client_help();
    exit(1);
}
