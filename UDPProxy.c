#include "UDPProxy.h"

#define PORT 8880
#define DPORT 8889

tuple_t *key; 
hashtable_t *ht;
void sighandler(int);

int main(int argc, char **argv) {
    struct sockaddr_in addr, dst;
    struct in_addr inaddr;
	int in_sock, new_sock, cur_sock, idx, fdmax, i, j, len = sizeof(addr);
	char buff[BUFF_LEN];
	char addr_buff[INET_ADDRSTRLEN];
	char *DEST_IP = "127.0.0.1";
	fd_set master, readfds;
    tuple_t *cur_tuple;

	/* prepare data structures for select() */
	FD_ZERO(&master);
	FD_ZERO(&readfds);

	key = calloc(1, sizeof(tuple_t));
	ht = new_ht();

	in_sock = bind_sock(INADDR_ANY, PORT);
	fcntl(in_sock, F_SETFL, O_NONBLOCK);

	/* add the listener to the master set */
	FD_SET(in_sock, &master);
	/* keep track of the biggest file descriptor */
	fdmax = in_sock;

	signal(SIGINT, sighandler);


	while(1) {
		printf("Ready\n");
		readfds = master; // copy it
		if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) {
			die("select");
		}
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &readfds)) {
				/* iptables redirected packet - may be new or not */
				if (i == in_sock) {
					if (recvfrom(in_sock, buff, BUFF_LEN, 0, (struct sockaddr\
									*)&addr, (socklen_t *)&len) < 0) {
						die("Receive error");
					}
					inet_ntop(AF_INET, &addr.sin_addr.s_addr, addr_buff, \
							INET_ADDRSTRLEN);
					printf("Received Packet from %s:%d\n", addr_buff, \
							ntohs(addr.sin_port));

					/* check for socket in table */
					addr_to_tuple(&addr, &dst, key);
					idx = contains(key, ht);
					/* new connection: create socket */
					if (idx == -1) {
						new_sock = bind_sock(INADDR_ANY, 0);
						fcntl(new_sock, F_SETFL, O_NONBLOCK);
						FD_SET(new_sock, &master);
						fdmax = new_sock;
						idx = add(key, new_sock, ht);
						printf("New connection added to table at %d\n", idx);
						print_table(ht);
					}
					cur_sock = ht->table[idx]->value;
                    /* prepare address for destination */
                    memset((char *) &dst, 0, sizeof(dst));
                    dst.sin_family = AF_INET;
                    dst.sin_port = htons(DPORT);
                    inet_aton(DEST_IP, &dst.sin_addr);
				}
				/* previously established connection coming in return direction */
				else {
					if (recvfrom(i, buff, BUFF_LEN, 0, (struct sockaddr *)&addr, \
								(socklen_t *)&len) < 0) {
						die("Receive error");
					}
					inet_ntop(AF_INET, &addr.sin_addr.s_addr, addr_buff, \
							INET_ADDRSTRLEN);
					printf("Received Packet from %s:%d\n", addr_buff, \
							ntohs(addr.sin_port));
					cur_sock = i;
					/* find destination from entry that contains this sock */
					for (j = 0; j < ht->capacity; j++) {
                        if (ht->table[j]) {
                            /* found the entry to match */
                            if (ht->table[j]->value == cur_sock) {
                                cur_tuple = ht->table[j]->key;
                                inaddr.s_addr = cur_tuple->src_ip;
                                /* convert ip from int to char * */
                                inet_ntop(AF_INET, &inaddr, addr_buff, \
                                        INET_ADDRSTRLEN);
                                /* prepare address for destination */
                                memset((char *) &dst, 0, sizeof(dst));
                                dst.sin_family = AF_INET;
                                dst.sin_port = cur_tuple->src_port;
                                inet_aton(addr_buff, &dst.sin_addr);
                                break;
                            }
                        }
					}

				}
				/* send */
				if (sendto(cur_sock, buff, BUFF_LEN, 0, \
							(struct sockaddr *)&dst, len) < 0) {
					die("Send Error");
				}
				inet_ntop(AF_INET, &dst.sin_addr.s_addr, addr_buff, \
						INET_ADDRSTRLEN);
				printf("Sent packet to: %s:%d\n", addr_buff, ntohs(dst.sin_port));
			}
		}
	}
}

void sighandler(int signum) {
	printf("Exiting...\n");
	free(key);
	destroy_ht(ht);
	exit(1);
}
