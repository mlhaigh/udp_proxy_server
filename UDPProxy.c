#include "UDPProxy.h"

#define PORT 8880
#define DPORT 8889

tuple_t *key;
hashtable_t *ht;
void sighandler(int);

int main(int argc, char **argv) {
    struct sockaddr_in addr, dst;
    int in_sock, new_sock, idx, len = sizeof(addr);
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];
    char *DEST_IP = "127.0.0.1";

    key = calloc(1, sizeof(tuple_t));
    ht = new_ht();

    in_sock = bind_sock(INADDR_ANY, PORT);

    signal(SIGINT, sighandler);

    /* prepare address for destination */
    memset((char *) &dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_port = htons(DPORT);
    inet_aton(DEST_IP, &dst.sin_addr);
    
    while(1) {
        printf("Ready\n");
        if (recvfrom(in_sock, buff, BUFF_LEN, 0, (struct sockaddr *)&addr,\
                  (socklen_t *)&len) < 0) {
            die("Receive error");
        }
        inet_ntop(AF_INET, &addr.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, ntohs(addr.sin_port));

        /* check for socket in table */
        addr_to_tuple(&addr, &dst, key);
        idx = contains(key, ht);
        /* new connection: create socket */
        if (idx == -1) {
            new_sock = bind_sock(INADDR_ANY, 0);
            idx = add(key, new_sock, ht);
            printf("New connection added to table at %d\n", idx);
            print_table(ht);
        }
        /* send */
        if (sendto(((ht->table[idx])->value), buff, BUFF_LEN, 0, \
                    (struct sockaddr *)&dst, len) < 0) {
            die("Send Error");
        }
        inet_ntop(AF_INET, &dst.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Sent packet to: %s:%d\n", addr_buff, ntohs(dst.sin_port));
    }
}

void sighandler(int signum) {
    printf("Exiting...\n");
    free(key);
    destroy_ht(ht);
    exit(1);
}
