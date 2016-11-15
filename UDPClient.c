#include "UDPProxy.h"

int main(int argc, char **argv) {

    struct sockaddr_in serv_addr, src_addr, recv, *cmsg_addr;
    int sock, port, clnt_port, num_pkts, i, recd, len = sizeof(serv_addr);
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct iovec msg_iov = {0};
    struct cmsghdr *cmsg_arr[CMSG_ARR_LEN];

    if(argc < 5) {
        die("Usage: UDPClient <server_ip> <server_port> <client_port> <num_packets>");
    }
    port = atoi(argv[2]);
    clnt_port = atoi(argv[3]);
    num_pkts = atoi(argv[4]);

    /* prepare msghdr for receiving */
    memset((char *) &msg, 0, sizeof(msg));
    /* set buffer for source address */
    msg.msg_name = &src_addr;
    msg.msg_namelen = 96;
    /* set array for data */
    msg_iov.iov_base = &buff;
    msg_iov.iov_len = BUFF_LEN;
    msg.msg_iov = &msg_iov;
    msg.msg_iovlen = 1;
    /* set array for cmsg / original dest */
    msg.msg_control = cmsg_arr;
    msg.msg_controllen = CMSG_ARR_LEN;

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
        if (sendto(sock, buff, 1400, 0, (struct sockaddr *)&serv_addr, len) < 0) {
            die("Send Error");
        }
        /* print addr info for testing */
        inet_ntop(AF_INET, &serv_addr.sin_addr.s_addr, addr_buff, \
                INET_ADDRSTRLEN); //get dest info
        printf("Packet %d sent to %s:%d\n", i, addr_buff, \
                ntohs(serv_addr.sin_port));

        /* receive reply */

        /* recd = recvmsg(sock, &msg, 0);
        if (recd < 0) {
            die("Receive error");
        }

        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; \
                cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if((cmsg->cmsg_level == SOL_IP) && \
                    (cmsg->cmsg_type == IP_RECVORIGDSTADDR)) {
                printf("message found\n");
                cmsg_addr = (struct sockaddr_in *)CMSG_DATA(cmsg);
                inet_ntop(AF_INET, &cmsg_addr->sin_addr.s_addr, addr_buff, \
                INET_ADDRSTRLEN);
        printf("Received Packet. Orig Dest:  %s:%d\n", addr_buff, \
                ntohs(cmsg_addr->sin_port)); 

                inet_ntop(AF_INET, &src_addr.sin_addr.s_addr, addr_buff, \
                INET_ADDRSTRLEN);
        printf("src:  %s:%d\n", addr_buff, ntohs(src_addr.sin_port)); 
            }
        } */
        
        if (recvfrom(sock, buff, BUFF_LEN, 0, (struct sockaddr *) &recv,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        } 
        inet_ntop(AF_INET, &recv.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, ntohs(recv.sin_port));
    }

    return 0;
}
