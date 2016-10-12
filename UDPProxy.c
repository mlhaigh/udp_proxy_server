#include "UDPProxy.h"

#define PORT 8880

char addr_buff[INET_ADDRSTRLEN];
char str_buff[STR_BUFF_LEN];

/* fill addr_buff with UDP 4-tuple as string for hashing */
void addr_to_str(struct sockaddr_in src) {
    int i;
    char src_port = (char) (src.sin_port & 0x255);
    inet_ntop(AF_INET, &src.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
    for (i = 0; i < 16; i++) {
        str_buff[i] = addr_buff[i];
    }
    for (i = 0; i < 5; i++) {
        str_buff[i + 15] = src_port[i]
    }
    /* figure out how to get destination information from packet and 
     * complete the string. For now cannot get destination port */
}

int main(int argc, char **argv) {

    int in_sock, out_sock, port, len, flags = 0x01;
    struct sockaddr_in addr, in, out;
    char buff[BUFF_LEN];

    /* set up receive socket */
    if ((in_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        die("Socket Error");
    }
    if (setsockopt(in_sock, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    }
    if (setsockopt(in_sock, IPPROTO_IP, IP_PKTINFO, &flags, sizeof(flags)) < 0) {
        die ("setsockopt error");
    }
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(in_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
        die("Socket bind failure");
    }

    /* set up hash table */
    GHashTable hash_table = g_hash_table_new(&g_str_hash, &g_str_equal);

    len = sizeof(in);

    while(1) {
        /* receive */
        if (recvfrom(in_sock, buff, BUFF_LEN, 0, (struct sockaddr *) &in,\
                    (socklen_t *)&len) < 0) {
            die("Receive error");
        }
        inet_ntop(AF_INET, &in.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
        printf("Received Packet from %s:%d\n", addr_buff, in.sin_port);
        /* get original destination */

        /* check if a socket exists */
        if (g_hash_table_contains()) {

        }

        /* if a socket exists, send packet */

        /* if no socket exists, create a new socket */
        memset((char *) &addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(in_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 ) {
            die("Socket bind failure");
        }

        /* forward */
        if (sendto(out_sock, buff, BUFF_LEN, 0, (struct sockaddr *)&client, len) < 0) {
            die("Send Error");
        }
    }

}
