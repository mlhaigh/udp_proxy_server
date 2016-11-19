#include "UDPProxy.h"

#define PORT 7777
#define REV_PORT 8888

tuple_t *key; 
hashtable_t *ht;
volatile int do_exit = 0;
void sighandler(int);

int main(int argc, char **argv) {
    struct sockaddr_in addr, src_addr, *cmsg_addr;
    struct in_addr inaddr;
	int in_sock, rev_sock, new_sock, cur_sock, idx, fdmax, i, j, timer_fd; 
    int res, recd, len = sizeof(addr);
	char buff[BUFF_LEN];
	char addr_buff[INET_ADDRSTRLEN];
	fd_set master, readfds;
    tuple_t *cur_tuple;
    struct itimerspec timer;
    ssize_t timer_read;
	uint64_t  exp;
    struct timeval last, now;
    double elapsed;
    entry_t *cur_entry;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct iovec msg_iov = {0};
    struct cmsghdr *cmsg_arr[CMSG_ARR_LEN];

	/* prepare data structures for select() */
	FD_ZERO(&master);
	FD_ZERO(&readfds);

	key = calloc(1, sizeof(tuple_t));
	ht = new_ht();
	signal(SIGINT, sighandler);

    /* add timer to master set */
    memset((char *) &timer, 0, sizeof(timer));
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    timer.it_value.tv_sec = 300; //change initial to 5 minutes
    timer.it_interval.tv_sec = 30; //change repeat to 30 sec
    timerfd_settime(timer_fd, 0, &timer, NULL);
    FD_SET(timer_fd, &master);
    printf("timer set to fd %d\n", timer_fd);
	
    /* add listeners to the master set */
	in_sock = bind_sock(INADDR_ANY, PORT); //forward direction
	fcntl(in_sock, F_SETFL, O_NONBLOCK);
	FD_SET(in_sock, &master);
    printf("listener port set to fd %d\n", in_sock);
    rev_sock = bind_sock(INADDR_ANY, REV_PORT);
	fcntl(rev_sock, F_SETFL, O_NONBLOCK);
	FD_SET(rev_sock, &master);
    printf("rev listener port set to fd %d\n", rev_sock);

	/* keep track of the biggest file descriptor */
	fdmax = rev_sock;

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

    /* get current time for first time comparison */
    gettimeofday(&last, NULL);

	while(1) {
		readfds = master; // copy select table
		res = select(fdmax+1, &readfds, NULL, NULL, NULL);
		if (res < 0 && errno != EINTR) {
            die("select");
        } else if (res == 0) { /* nothing ready */
            sleep(.001);
        } else if (do_exit) {
            /* ctrl-c pressed. Clean up and exit */
            printf("Exiting...\n");
            free(key);
            destroy_ht(ht);
            printf("exit finished\n");
            exit(1);
        }
        /* get time elapsed */
        gettimeofday(&now, NULL);
        /* elapsed = microseconds elapsed since last update */
        elapsed = (((now.tv_sec - last.tv_sec) * 1000000) + \
                (now.tv_usec - last.tv_usec)); 
        last = now;
        //printf("Microseconds elapsed: %f\n", elapsed);

        /* fill buckets */
        if (ht->size > 0) {
            for (i = 0; i < ht->capacity; i++) {
                if (ht->table[i]) {
                    /* printf("filling bucket at %d. counter: %d. elapsed: %f. \
                            elapsed*rate: %f\n", i, ht->table[i]->counter, \
                            elapsed, (elapsed * ht->table[i]->rate)); */
                    ht->table[i]->counter = MIN((ht->table[i]->counter + \
                                elapsed * ht->table[i]->rate), TOKEN_MAX);
                }
            }
        }

        /* deal with active file descriptors */
		for(i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &readfds)) {
                printf("select: %d\n", i);
                recd = 0;
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
                    printf("in_sock\n");
                    recd = recvmsg(in_sock, &msg, 0);
                    if (recd < 0) {
						die("Receive error");
					}

                    /* find original destination */
                    printf("checking for original dst\n");
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
                    }

                    /* filter spotify spam packets */
                    if ((ntohs(cmsg_addr->sin_port) == 57621) && \
                            (ntohs(src_addr.sin_port) == 57621)) {
                        printf("spotify packet\n");
                        recd = 0; //so we dont try to send
                        break;
                    }

					/* check for socket in table */
					addr_to_tuple(&src_addr, cmsg_addr, key);
					idx = contains(key, ht);
					/* new connection: create socket */
					if (idx == -1) {
						new_sock = bind_sock(INADDR_ANY, 8888);
						fcntl(new_sock, F_SETFL, O_NONBLOCK);
						FD_SET(new_sock, &master);
						fdmax = new_sock;
						idx = add(key, new_sock, &src_addr, ht);
						//printf("New connection added to table at %d\n", idx);
						//print_table(ht);
					} 
                    /* conn exists - update timer and decrement counter */
                    else { 
                        ht->table[idx]->last_use = time(NULL);
                        ht->table[idx]->counter -= recd;
//TODO: COUNTER NEVER CHECKED FOR ZERO SINCE PACKET IS ALREADY BUFFERED
                    }
					cur_sock = ht->table[idx]->value;
				}
				/* previously established connection coming in return direction */
				else {
					cur_sock = i;
                    /* get entry in hash table */
                    for (j = 0; j < ht->capacity; j++) {
                        if (ht->table[j]) {
                            if (ht->table[j]->value == cur_sock) {
                                cur_entry = ht->table[j];
                                continue;
                            }
                        }
                    }
                    //printf("retrieved entry. Counter: %d\n", cur_entry->counter);

                    /* receive only if token is not full */
                    if (cur_entry->counter > 0) {
                        recd = recvmsg(in_sock, &msg, 0);
                        if (recd < 0) {
                            die("Receive error");
                        }

                        /* find original destination */
                        cmsg_addr = &cur_entry->orig_src;
                        inet_ntop(AF_INET, &cmsg_addr->sin_addr.s_addr, addr_buff, \
                            INET_ADDRSTRLEN);
                        printf("Received Packet. Orig Dest:  %s:%d\n", addr_buff, \
                            ntohs(cmsg_addr->sin_port)); 
                        inet_ntop(AF_INET, &src_addr.sin_addr.s_addr, addr_buff, \
                            INET_ADDRSTRLEN);
                        printf("src:  %s:%d\n", addr_buff, ntohs(src_addr.sin_port)); 

                        /* update timer and counter*/
                        cur_entry->last_use = time(NULL);
                        cur_entry->counter -= recd;
                        cur_tuple = cur_entry->key;
                        inaddr.s_addr = cur_tuple->src_ip;
                        /* convert ip from int to char * */
                        inet_ntop(AF_INET, &inaddr, addr_buff, \
                                INET_ADDRSTRLEN);
                    }
                    //TODO: BUFFER SINCE CANNOT RECEIVE

				}
                /* if data was received, send */
                if (recd > 0) {
                    if (sendto(cur_sock, buff, 1400, 0, \
                                (struct sockaddr *)cmsg_addr, len) < 0) {
                        die("Send Error");
                    }
                    inet_ntop(AF_INET, &cmsg_addr->sin_addr.s_addr, addr_buff, \
                            INET_ADDRSTRLEN);
                    printf("Sent packet to: %s:%d\n", addr_buff, \
                            ntohs(cmsg_addr->sin_port));
                }
			}
		} //end select for loop
	}
}

/* Clean up on ctrl-c */
void sighandler(int signum) {
    do_exit = 1;
}

