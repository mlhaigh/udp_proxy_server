#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>

#define BUFF_LEN 1024

void die(char *msg) {
    perror(msg);
    exit(1);
}

