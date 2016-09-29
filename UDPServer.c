#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>

#define BUFF_LEN 1024

void die(char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char **argv) {
    int client_fd, server_fd;
    char buffer[BUFF_LEN];
    
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    return 0;
}
