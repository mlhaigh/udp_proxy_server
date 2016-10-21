#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
//#include <netinet/ip.h>
#include <linux/netfilter_ipv4.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>

#define BUFF_LEN 1500
#define DST_PORT 8889
#define DST_IP "127.0.0.1"
#define TUPLE_SZ 20

void die(char *msg) {
    perror(msg);
    exit(1);
}

typedef struct tuple {
    unsigned long src_ip;
    unsigned short src_port;
    unsigned long dst_ip;
    unsigned short dst_port;
} tuple_t;

/* assume long is 8 bytes, short 2 */
void addr_to_tuple(struct sockaddr_in *src, struct sockaddr_in *dst, \
        tuple_t *res) {
    printf("addr_to_tuple\n");
    struct in_addr *dst_addr = malloc(sizeof(struct in_addr));
    res->src_ip = src->sin_addr.s_addr;
    printf("src_ip: %u\n", res->src_ip);
    res->src_port = src->sin_port;
    printf("src_port: %u\n", res->src_port);
    inet_pton(AF_INET, DST_IP, dst_addr);
    printf("pton\n");
    res->dst_ip = dst_addr->s_addr;
    printf("dst_addr: %u\n", res->dst_ip);
    res->dst_port = htons(DST_PORT);
    printf("end addr_to_tuple\n");
    free(dst_addr);
}

int compare_tuple(tuple_t *a, tuple_t *b) {
    if (((a->src_ip == b->src_ip) && (a->src_port == b->src_port) && \
        (a->dst_ip == b->dst_ip) && (a->dst_port == b->dst_port)) || \
        ((a->src_ip == b->dst_ip) && (a->src_port == b->dst_port) && \
         (a->dst_ip == b->src_ip) && (a->dst_port == b->src_port))) {
        return 1;
    }
    return 0;
}

void copy_tuple(tuple_t *src, tuple_t *dst) {
    dst->src_ip = src->src_ip;
    dst->src_port = src->src_port;
    dst->dst_ip = src->dst_ip;
    dst->dst_port = src->dst_port;
}

int bind_sock(int ip, int port) {
	int fd, flags = 0x01;
	struct sockaddr_in addr;

    memset((char *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(port);

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
	if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		die("Bind error");
	}
    printf("bound socket to %s:%d\n", ip, port);
	return fd;
}
