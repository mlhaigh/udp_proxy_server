#include "UDPProxy.h"

int main(int argc, char **argv) {

    struct sockaddr_in serv_addr, recv;
    int sock, port, clnt_port, num_pkts, i, len = sizeof(serv_addr);
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];

    if(argc < 5) {
        die("Usage: UDPClient <server_ip> <server_port> <client_port> <num_packets>");
    }
    port = atoi(argv[2]);
    clnt_port = atoi(argv[3]);
    num_pkts = atoi(argv[4]);

    /* prepare address for destination */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_aton(argv[1], &serv_addr.sin_addr) == 0) {
        die("Invalid IP entered");
    }

    sock = bind_sock(INADDR_ANY, clnt_port);

    /* initialize buffer */
    memset((char *) &buff, 0, sizeof(buff));
    
    for(i = 0; i < num_pkts; i++) {
        sprintf(buff, "Packet %d\n", i);
        if (sendto(sock, buff, BUFF_LEN, 0, (struct sockaddr *)&serv_addr, len) < 0) {
            die("Send Error");
        }
        /* print addr info for testing */
        inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, addr_buff, \
                INET_ADDRSTRLEN); //get dest info
        printf("Packet %d sent to %s:%d\n", i, addr_buff, \
                ntohs(serv_addr.sin_port));

        /* receive reply */
        if (recvfrom(sock, buff, BUFF_LEN, 0, (struct sockaddr *) &recv,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        } 
        inet_ntop(AF_INET, &recv.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, ntohs(recv.sin_port));
    }

    return 0;
}
