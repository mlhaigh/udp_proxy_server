#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
//#include <netinet/ip.h>
#include <linux/netfilter_ipv4.h>

#define BUFF_LEN 1500
#define STR_BUF_LEN 46 //16x2 for IP, 5x2 for port

void die(char *msg) {
    perror(msg);
    exit(1);
}

