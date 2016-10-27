#include "UDPProxy.h"

void die(char *msg) {
    perror(msg);
    exit(1);
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
    printf("Bound socket to %d:%d\n", ip, port);
	return fd;
}

