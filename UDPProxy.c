#include "UDPProxy.h"

tuple_t *key; 
hashtable_t *ht;
volatile int do_exit = 0;
void sighandler(int);

/* return 1 if original direction, 0 if reverse */
int check_direction(struct sockaddr_in *src, struct sockaddr_in *dst, int port) {
    if (ntohs(dst->sin_port) == port) {
        return 0;
    }
    return 1;
}

/* returns difference between last and now in nanoseconds 
 * and updates last to now */
double get_diff(struct timespec *last) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double diff = now.tv_nsec - last->tv_nsec;
    diff += (now.tv_sec - last->tv_sec) * 1000000000;
    last->tv_sec = now.tv_sec;
    last->tv_nsec = now.tv_nsec;
    return diff;
}

/* elapsed = nanoseconds */
void generate_token(hashtable_t *ht, char *buf, double elapsed, int sock, FILE *log) {
    if (ht->size > 0) {
        int i;
        entry_t *cur;
        for (i = 0; i < ht->capacity; i++) {
            if (ht->table[i]) {
                cur = ht->table[i];
                /* increment counters */
                print_log(log, "generate: s_ctr:%d d_ctr:%d elapsed:%f\n", \
                        cur->s_ctr, cur->d_ctr, elapsed);
                cur->s_ctr = MIN((TOKEN_MAX/2), \
                        (cur->s_ctr + ((elapsed * cur->rate) / 1000)));
                cur->d_ctr = MIN((TOKEN_MAX/2), \
                        (cur->d_ctr + ((elapsed * cur->rate) / 1000)));
                print_log(log, "updated s_ctr:%d d_ctr:%d\n", cur->s_ctr, cur->d_ctr);
            }
        }
    }
}

int add_sock(int port, fd_set *master, int *fdmax) {
    /* add listener socket to the master set */
    int sock = bind_sock(INADDR_ANY, port); //forward direction
    fcntl(sock, F_SETFL, O_NONBLOCK);
    FD_SET(sock, master);
    /* keep track of the biggest file descriptor */
    *fdmax = sock;
    return sock;
}

int main(int argc, char **argv) {
    struct sockaddr_in addr, src_addr, *cmsg_addr, *dst_addr;
    int sock, new_sock, cur_sock, idx, fdmax, i, j, timer_fd; 
    int res, recd, len = sizeof(addr);
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];
    fd_set master, readfds;
    struct itimerspec timer;
    ssize_t timer_read;
    uint64_t  exp;
    struct timespec last;
    struct msghdr msg;
    struct cmsghdr *cmsg;
    struct iovec msg_iov = {0};
    struct cmsghdr *cmsg_arr[CMSG_ARR_LEN];
    entry_t *cur_entry;
    FILE *log;
    unsigned short port;

    if(argc < 3) {
        die("Usage: argv[0] <port> <log_file>");
    }

    log = fopen(argv[2], "w");
    if (log == 0) {
        die("error opening file");
    }

    port = atoi(argv[1]);

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

    sock = add_sock(port, &master, &fdmax);

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
    //gettimeofday(&last, NULL);
    clock_gettime(CLOCK_REALTIME, &last);

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
            if (fclose(log) != 0) {
                die("error closing log file");
            }
            printf("exit finished\n");
            exit(1);
        }

        generate_token(ht, buff, get_diff(&last), sock, log);

        /* deal with active file descriptors */
        for(i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &readfds)) {
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
                                destroy_entry(ht->table[j]);
                                ht->table[j] = 0;
                            }
                        }
                    }
                }
                /* iptables redirected packet - may be new or not */
                else if (i == sock) {
                    recd = recvmsg(sock, &msg, 0);
                    if (recd < 0) {
                        die("Receive error");
                    }

                    /* find original destination */
                    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; \
                            cmsg = CMSG_NXTHDR(&msg, cmsg)) {
                        if((cmsg->cmsg_level == SOL_IP) && \
                                (cmsg->cmsg_type == IP_RECVORIGDSTADDR)) {
                            cmsg_addr = (struct sockaddr_in *)CMSG_DATA(cmsg);
                            /*inet_ntop(AF_INET, &cmsg_addr->sin_addr.s_addr, addr_buff, \
                                    INET_ADDRSTRLEN);
                             printf("Received Packet. Orig Dest:  %s:%d\n", addr_buff, \
                                    ntohs(cmsg_addr->sin_port)); 
                            inet_ntop(AF_INET, &src_addr.sin_addr.s_addr, addr_buff, \
                                    INET_ADDRSTRLEN);
                            printf("src:  %s:%d\n", addr_buff, \
                            ntohs(src_addr.sin_port));*/ 
                        }
                    }

                    /* Ignore broadcast due to permission error */
                    if(((cmsg_addr->sin_addr.s_addr >> 24) & 255) == 255) {
                        recd = 0;
                        break;
                    }

                    dst_addr = cmsg_addr;
                    /* check for socket in table */
                    addr_to_tuple(cmsg_addr, key);
                    idx = contains(key, ht);
                    /* new connection: create entry */
                    if (idx == -1) {
                        new_sock = add_sock(0, &master, &fdmax);
                        cur_sock = new_sock;
                        idx = add(key, &src_addr, cmsg_addr, new_sock, ht);
                    } 
                    /* conn exists - update timer and decrement counter */
                    else {
                        cur_entry = ht->table[idx];
                        cur_sock = cur_entry->sock;
                        cur_entry->last_use = time(NULL);
                        /* only send if ctr > 0 */
                        print_log(log, "orig dir prev s_ctr:%d\n", cur_entry->s_ctr);
                        if (cur_entry->s_ctr >= 0) {
                            cur_entry->s_ctr -= recd;
                        }
                        /* counter < 0 */
                        else {
                            recd = 0;
                        }
                        print_log(log, "orig dir after s_ctr:%d\n", cur_entry->s_ctr);
                    }
                } //end in_sock

                else  { //another socket = rev direction already established
                    recd = recvmsg(i, &msg, 0);
                    if (recd < 0) {
                        die("Receive error");
                    }

                    /* find original destination */
                    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; \
                            cmsg = CMSG_NXTHDR(&msg, cmsg)) {
                        if((cmsg->cmsg_level == SOL_IP) && \
                                (cmsg->cmsg_type == IP_RECVORIGDSTADDR)) {
                            cmsg_addr = (struct sockaddr_in *)CMSG_DATA(cmsg);
                            /* inet_ntop(AF_INET, &cmsg_addr->sin_addr.s_addr, addr_buff, \
                                    INET_ADDRSTRLEN);
                            printf("Received Packet. Orig Dest:  %s:%d\n", addr_buff, \
                                    ntohs(cmsg_addr->sin_port)); 
                            inet_ntop(AF_INET, &src_addr.sin_addr.s_addr, addr_buff, \
                                    INET_ADDRSTRLEN);
                            printf("src:  %s:%d\n", addr_buff, \
                            ntohs(src_addr.sin_port)); */
                        }
                    }

                    /* check for socket in table */
                    addr_to_tuple(&src_addr, key);
                    idx = contains(key, ht);
                    if (idx < 0) {
                        die("rev direction could not find entry");
                    }
                    cur_entry = ht->table[idx];
                    cur_sock = cur_entry->sock;
                    /* get original source addr */
                    dst_addr = &(cur_entry->orig_src);
                    /* update time and counter */
                    cur_entry->last_use = time(NULL);
                    /* only send if ctr > 0 */
                    print_log(log, "rev dir prev d_ctr:%d\n", cur_entry->d_ctr);
                    if (cur_entry->d_ctr > 0) {
                        ht->table[idx]->d_ctr -= recd;
                    }
                    /* ctr < 0 */
                    else {
                        recd = 0;
                    }
                    print_log(log, "rev dir after d_ctr:%d\n", cur_entry->d_ctr);
                } //end rev direction sock

                /* if data was received, send */
                if (recd > 0) {
                    if (sendto(cur_sock, buff, 1400, 0, \
                                (struct sockaddr *)dst_addr, len) < 0) {
                        die("Send Error");
                    }
                    /* inet_ntop(AF_INET, &dst_addr->sin_addr.s_addr, addr_buff, \
                            INET_ADDRSTRLEN);
                    printf("sent packet to:  %s:%d\n", addr_buff, \
                            ntohs(dst_addr->sin_port)); */
                    print_log(log, "sent packet. len:%d\n", recd);
                }
                else {
                    print_log(log, "no packet sent");
                }
            } //end select case?
        } //end select for loop?
    } //end while(1)?
}//end main

    /* Clean up on ctrl-c */
    void sighandler(int signum) {
        do_exit = 1;
    }

