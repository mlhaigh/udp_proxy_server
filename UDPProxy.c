#include "UDPProxy.h"

#define PORT 8880
#define DPORT 8889

tuple_t *key; 
hashtable_t *ht;
volatile int do_exit = 0;
void sighandler(int);

int main(int argc, char **argv) {
    struct sockaddr_in addr, dst;
    struct in_addr inaddr;
	int in_sock, new_sock, cur_sock, idx, fdmax, i, j, timer_fd;
    int len = sizeof(addr);
	char buff[BUFF_LEN];
	char addr_buff[INET_ADDRSTRLEN];
	char *DEST_IP = "127.0.0.1";
	fd_set master, readfds;
    tuple_t *cur_tuple;
    struct itimerspec timer;
	uint64_t timer_read, exp;

	/* prepare data structures for select() */
	FD_ZERO(&master);
	FD_ZERO(&readfds);

	key = calloc(1, sizeof(tuple_t));
	ht = new_ht();
	signal(SIGINT, sighandler);

    /* add timer to master set */
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    timer.it_value.tv_sec = 5; //change initial to 4 minutes
    timer.it_interval.tv_sec = 5; //change repeat to 30 sec
    timerfd_settime(timer_fd, 0, &timer, NULL);
    FD_SET(timer_fd, &master);
	
    /* add the listener to the master set */
	in_sock = bind_sock(INADDR_ANY, PORT);
	fcntl(in_sock, F_SETFL, O_NONBLOCK);
	FD_SET(in_sock, &master);
	/* keep track of the biggest file descriptor */
	fdmax = in_sock;

	while(1) {
        /* ctrl-c pressed. Clean up and exit */
        if (do_exit) {
            printf("Exiting...\n");
            free(key);
            destroy_ht(ht);
        }
		printf("Ready\n");
		readfds = master; // copy it
		if (select(fdmax+1, &readfds, NULL, NULL, NULL) == -1) {
			die("select");
		}
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &readfds)) {
                /* timer set: check timeout values */
                if (i == timer_fd) {
					timer_read = read(timer_fd, &exp, sizeof(uint64_t));
					if (timer_read  != sizeof(uint64_t)) {
                   		die("read");
					}
                    time_t now = time(NULL);
                    for (j = 0; j < ht->capacity; j++) {
                        if (ht->table[j]) {
                            /* time out */
                            if (difftime(now, ht->table[j]->last_use) > TIME_OUT) {
                                /* clear from select table */
                                FD_CLR(ht->table[j]->value, &master);
                                destroy_entry(ht->table[j]);
                                ht->table[j] = 0;
                            }
                        }
                    }
                }
				/* iptables redirected packet - may be new or not */
                else if (i == in_sock) {
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
                    /* connection already exists - update timer */
                    else { 
                        ht->table[idx]->last_use = time(NULL);
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
                                /* update timer */
                                ht->table[j]->last_use = time(NULL);
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
                /* if not timer, send */
                if (i != timer_fd) {
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
}

/* does not work with select */
void sighandler(int signum) {
    do_exit = 1;
}

