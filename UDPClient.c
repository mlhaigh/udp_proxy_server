#include "UDPProxy.h"

int main(int argc, char **argv) {

    struct sockaddr_in client;
    int sock, port, i, num_pkts, len = sizeof(client);
    char buff[BUFF_LEN];

    if(argc > 4) {
        die("Usage: UDPProxy <server_ip> <server_port> <num_packets>");
    }
    port = atoi(argv[2]);
    num_pkts = atoi(argv[3]);

    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
        die("Socket error");
    }

    memset((char *) &client, 0, sizeof(client));
    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    if (inet_aton(argv[1], &client.sin_addr) == 0) {
        die("Invalid IP entered");
    }

    for(i = 0; i < num_pkts; i++) {
        sprintf(buff, "Packet %d\n", i);
        if (sendto(sock, buff, BUFF_LEN, 0, (struct sockaddr *)&client, len) < 0) {
            die("Send Error");
        }
    }

    return 0;
}
