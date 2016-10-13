#include "UDPProxy.h"

int main(int argc, char **argv) {
    int sock, port, len, flags = 0x01;
    struct sockaddr_in addr, client;
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];

    if(argc < 2) {
        die("Usage: UDPServer <port>");
    }
    port = atoi(argv[1]);

    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        die("Socket Error");
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    }
    
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
        die("Socket bind failure");
    }

    /* printing socket info for testing */
    inet_ntop(AF_INET, &addr.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
    printf("Listening on %s:%d\n", addr_buff, ntohs(addr.sin_port));

    len = sizeof(client);

    while(1) {
        if (recvfrom(sock, buff, BUFF_LEN, 0, (struct sockaddr *) &client,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        }
        
        inet_ntop(AF_INET, &client.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, ntohs(client.sin_port));
        //getsockopt(sock, SOL_IP, SO_ORIGINAL_DST, &addr, &len);
        //inet_ntop(AF_INET, &addr, addr_buff, INET_ADDRSTRLEN);
        //printf("Original Dest: %s:%d\n", addr_buff, ntohs(addr.sin_port));
    }
 
    return 0;
}
