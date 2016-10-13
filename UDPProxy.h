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
#define STR_BUF_LEN 42 //16x2 for IP, 5x2 for port

void die(char *msg) {
    perror(msg);
    exit(1);
}

/* fill buff with UDP 4-tuple as string for hashing */
//void addr_to_str(struct sockaddr_in src, char *buff) {
//    int i;
//    char src_port = (char) (src.sin_port & 0x255);
//    inet_ntop(AF_INET, &src.sin_addr.s_addr, addr_buff, INET_ADDRSTRLEN);
//    for (i = 0; i < 16; i++) {
//        str_buff[i] = addr_buff[i];
//    }
//    for (i = 0; i < 5; i++) {
//        str_buff[i + 15] = src_port[i];
//    }
//    /* figure out how to get destination information from packet and 
//     * complete the string. For now cannot get destination. */
//    for (i = 0; i < 16; i++) {
//        str_buff[i + 21] = DEST_IP[i] 
//    }
//    for (i = 0; i < 5; i++) {
//        str_buff[i + 37] = DEST_PORT[i];
//    }
//    printf("addr_to_str: %s\n", str_buff);
//}

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
