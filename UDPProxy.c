#include "UDPProxy.h"

#define PORT 9990

int main(int argc, char **argv) {
    
    int sock, port, len, flags = 0x01;
    struct sockaddr_in addr, in, out;
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        die("Socket Error");
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    }
    
   /* if (setsockopt(sock, SOL_IP, IP_RECVORIGDSTADDR, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    } */

    if (setsockopt(sock, IPPROTO_IP, IP_RECVORIGDSTADDR, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    }
    
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
        die("Socket bind failure");
    }

    len = sizeof(in);

    while(1) {
        if (recvfrom(sock, buff, BUFF_LEN, 0, (struct sockaddr *) &in,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        }
        
        inet_ntop(AF_INET, &in.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, in.sin_port);


    }



}
