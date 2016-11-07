#include "UDPProxy.h"

/* print error message and exit */
void die(char *msg) {
    perror(msg);
    exit(1);
}

/* returns fd for socket bound to ip:port */
int bind_sock(int ip, int port) {
	int fd, flags = 0x01;
	struct sockaddr_in addr;

    memset((char *) &addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(ip);
	addr.sin_port = htons(port);

	fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (fd < 0) {
        die("Socket error");
    }
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(int));
    /* for tproxy */
    setsockopt(fd, SOL_IP, IP_RECVORIGDSTADDR, &flags, sizeof(int));
    setsockopt(fd, SOL_IP, IP_TRANSPARENT, &flags, sizeof(int));
	if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		die("Bind error");
	}
    printf("Bound socket to %d:%d\n", ip, port);
	return fd;
}

