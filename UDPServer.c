#include "UDPProxy.h"

int main(int argc, char **argv) {
    struct sockaddr_in client;
    int sock, port, len = sizeof(client);
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];

    if(argc < 2) {
        die("Usage: UDPServer <port>");
    }
    port = atoi(argv[1]);

    sock = bind_sock(INADDR_ANY, port);

    while(1) {
        /* receive a packet */
        if (recvfrom(sock, buff, BUFF_LEN, 0, (struct sockaddr *) &client,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        } 
        inet_ntop(AF_INET, &client.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, ntohs(client.sin_port));
        
        /* send response packet */
        if (sendto(sock, buff, BUFF_LEN, 0, (struct sockaddr *)&client, len) \
                < 0) {
            die("Send Error");
        }
        inet_ntop(AF_INET, &client.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Sent response packet to %s:%d\n", addr_buff, \
                ntohs(client.sin_port));
    } 
    return 0;
}

