#include "UDPProxy.h"

#define PORT 8880
#define DPORT 8889

/* temporary destination address since I cannot get them from the packet */

int main(int argc, char **argv) {
    int in_sock, out_sock, flags = 0x01;
    struct sockaddr_in addr, dst;
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];
    int len = sizeof(addr);
    char *DEST_IP = "127.0.0.1";

    in_sock = bind_sock(INADDR_ANY, PORT);
    out_sock = bind_sock(INADDR_ANY, 0);

    while(1) {
        printf("waiting for a packet\n");
        if (recvfrom(in_sock, buff, BUFF_LEN, 0, (struct sockaddr *)&addr,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        }
        inet_ntop(AF_INET, &addr.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, ntohs(addr.sin_port));

        /* prepare address for destination */
        memset((char *) &dst, 0, sizeof(dst));
        dst.sin_family = AF_INET;
        dst.sin_port = htons(DPORT);
        inet_aton(DEST_IP, &dst.sin_addr);

        if (sendto(out_sock, buff, BUFF_LEN, 0, (struct sockaddr *)&dst, len) < 0) {
            die("Send Error");
        }
        inet_ntop(AF_INET, &dst.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Sent packet to: %s:%d\n", addr_buff, ntohs(dst.sin_port));
    }
}
