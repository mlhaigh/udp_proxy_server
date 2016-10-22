#include "UDPProxy.h"

int main(int argc, char **argv) {

    struct sockaddr_in serv_addr, clnt_addr;
    int sock, port, clnt_port, num_pkts, i, len = sizeof(serv_addr);
    int flags = 0x01;
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];
    char addr_buff2[INET_ADDRSTRLEN];

    if(argc < 5) {
        die("Usage: UDPClient <server_ip> <server_port> <client_port> <num_packets>");
    }
    port = atoi(argv[2]);
    clnt_port = atoi(argv[3]);
    num_pkts = atoi(argv[4]);

    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        die("Socket error");
    }
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    }
/*    if (setsockopt(sock, SOL_IP, SO_ORIGINAL_DST, &flags, sizeof(flags)) < 0) {
        die ("setsockopt orig_dst error");
    } */

    /* prepare address for destination */
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_aton(argv[1], &serv_addr.sin_addr) == 0) {
        die("Invalid IP entered");
    }

    /* prepare address for sending (so we can get port#) */
    memset((char *) &clnt_addr, 0, sizeof(clnt_addr));
    clnt_addr.sin_family = AF_INET;
    clnt_addr.sin_port = htons(clnt_port); 
    clnt_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    /* for now use bind() so I can see which port it was sent from for testing */
    if (bind(sock, (struct sockaddr *)&clnt_addr, sizeof(clnt_addr)) < 0 ) {
        die("Socket bind failure");
    }

    for(i = 0; i < num_pkts; i++) {
        sprintf(buff, "Packet %d\n", i);
        if (sendto(sock, buff, BUFF_LEN, 0, (struct sockaddr *)&serv_addr, len) < 0) {
            die("Send Error");
        }
        /* print addr info for testing */
        inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, addr_buff, \
                INET_ADDRSTRLEN); //get dest info
        inet_ntop(AF_INET, &clnt_addr.sin_addr.s_addr, addr_buff2, \
                INET_ADDRSTRLEN); //get src info (for test)
        printf("Packet %d sent from %s:%d to %s:%d\n", i, addr_buff2, \
                ntohs(clnt_addr.sin_port), addr_buff, \
                ntohs(serv_addr.sin_port));
    }

    return 0;
}
