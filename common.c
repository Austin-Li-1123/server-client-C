/**
 * Nonstop Networking
 * CS 241 - Spring 2020
 */
#include "common.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>



ssize_t read_all_from_socket(int socket, char *buffer, size_t count) {
    // Your Code Here
    //code inspired by lecture codes
    size_t total = 0;
    ssize_t r;
    while(total < count && (r=read(socket, buffer+total, count-total))) {
        if (r == -1 && EINTR == errno) {
            continue;
        }
        if (r == -1) {
            return -1;
        }
        total += r;
    }

    return total;
}

ssize_t write_all_to_socket(int socket, const char *buffer, size_t count) {
    //fprintf(stderr, "running write_all_to_socket\n");
    // Your Code Here
    //code inspired by lecture codes
    size_t total = 0;
    ssize_t r;
    while(total < count && (r=write(socket, buffer+total, count-total))) {
        if (r == -1 && EINTR == errno) {
            continue;
        }
        if (r == -1) {
            return -1;
        }
        total += r;
    }

    return total;
}