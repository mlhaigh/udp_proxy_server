#include "UDPProxy.h"

tuple_t *key; 
hashtable_t *ht;
volatile int do_exit = 0;
void sighandler(int);

/* returns difference between last and now in nanoseconds  */
double get_diff(struct timespec *last) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double diff = now.tv_nsec - last->tv_nsec;
    diff += (now.tv_sec - last->tv_sec) * 1000000000;
    last->tv_sec = now.tv_sec;
    last->tv_nsec = now.tv_nsec;
    return diff;
}

/* for each entry in ht, increment available token/bandwidth based on time 
 * elapsed (elapsed = nanoseconds). Prints counters before and after to log */
void generate_token(hashtable_t *ht, char *buf, double elapsed, int sock, FILE *log) {
    if (ht->size > 0) {
        int i;
        entry_t *cur;
        for (i = 0; i < ht->capacity; i++) {
            if (ht->table[i]) {
                cur = ht->table[i];
                /* increment counters */
                print_log(log, "Generating token: Current counters: s_ctr:%d \
                        d_ctr:%d elapsed:%f\n", cur->s_ctr, cur->d_ctr, elapsed);
                cur->s_ctr = MIN((TOKEN_MAX/2), \
                        (cur->s_ctr + ((elapsed * cur->rate) / 1000)));
                cur->d_ctr = MIN((TOKEN_MAX/2), \
                        (cur->d_ctr + ((elapsed * cur->rate) / 1000)));
                print_log(log, "Updated counters: s_ctr:%d d_ctr:%d\n", \
                        cur->s_ctr, cur->d_ctr);
            }
        }
    }
}

/* bind a socket on port and add it to master set for select() */
int add_sock(int port, fd_set *master, int *fdmax) {
    /* add listener socket to the master set */
    int sock = bind_sock(INADDR_ANY, port);
    fcntl(sock, F_SETFL, O_NONBLOCK);
    FD_SET(sock, master);
    /* keep track of the biggest file descriptor */
    *fdmax = sock;
    return sock;
}

/* parse configuration file and populate config_t with parameters */
void read_config(char *filename, config_t *config) {
    char buff[LINE_SZ] = {0};
    int i = 0;
    char *token;
    struct sockaddr_in addr;
    tuple_t tuple;
    FILE *file = fopen(filename, "r");
    if (file == 0) {
        die("error opening config file");
    }
    while (fgets(buff, sizeof(buff), file) > 0) {
        token = strtok(buff, " ");
        if (token == 0) {
            die("error reading config file");
        }
        inet_aton(token, &addr.sin_addr);
        token = strtok(buff, " ");
        if (token == 0) {
            die("error reading config file");
        }
        addr.sin_port = *token;
        addr_to_tuple(&addr, &tuple);
        copy_tuple(&tuple, &config->tuples[i]);
        token = strtok(buff, " ");
        if (token == 0) {
            die("error reading config file");
        }
        config->rates[i++] = atoi(token);
    }
}

/* checks the configuration file for a tuple and returns the configured rate
 * returns 0 if tuple is not in configuration */
int config_get_rate(tuple_t *tuple, config_t *config) {
    int i;
    for (i = 0; i < CONFIG_MAX; i++) {
        if (compare_tuple(tuple, &config->tuples[i])) {
            return config->rates[i];
        }
    }
    return 0;
}

/* for reverse direction new connection. Check if the hash returns the correct
 * socket. If not, check the hash table for an entry with a matching socket */
entry_t *get_entry_rev(int sock, struct sockaddr_in *src_addr, hashtable_t *ht) {
    int i;
    addr_to_tuple(src_addr, key);
    i = contains(key, ht);
    if (i < 0) {
        die("error getting reverse entry");
    }
    if (ht->table[i]->sock == sock) {
        return ht->table[i];
    }
    for (i = 0; i < ht->capacity; i++) {
        if (ht->table[i]->sock == sock) {
            return ht->table[i];
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    struct sockaddr_in addr, src_addr, *cmsg_addr, *dst_addr;
    int sock, new_sock, cur_sock, idx, fdmax, i, j, timer_fd; 
    int res, recd, len = sizeof(addr);
    char buff[BUFF_LEN];
    //char addr_buff[INET_ADDRSTRLEN];
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
    config_t config;

    memset(&config, 0, sizeof(config));

    if(argc < 3) {
        printf("Usage: %s <port> <log_file> <config_file>\n", argv[0]);
        exit(1);
    }

    port = atoi(argv[1]);
    log = fopen(argv[2], "w");
    if (log == 0) {
        die("error opening file");
    }
    read_config(argv[3], &config);

    /* prepare data structures for select() */
    FD_ZERO(&master);
    FD_ZERO(&readfds);

    key = calloc(1, sizeof(tuple_t));
    ht = new_ht();
    signal(SIGINT, sighandler);

    /* add timer to master set for socket timeout */
    memset((char *) &timer, 0, sizeof(timer));
    timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    timer.it_value.tv_sec = 300; //change initial to 5 minutes
    timer.it_interval.tv_sec = 30; //change repeat to 30 sec
    timerfd_settime(timer_fd, 0, &timer, NULL);
    FD_SET(timer_fd, &master);

    /* main listening socket for incoming connections */
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
    clock_gettime(CLOCK_REALTIME, &last);

    while(1) {
        readfds = master; // copy select table
        res = select(fdmax+1, &readfds, NULL, NULL, NULL);
        if (res < 0 && errno != EINTR) {
            die("select error");
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
            exit(1);
        }

        generate_token(ht, buff, get_diff(&last), sock, log);

        /* deal with active file descriptors */
        for(i = 0; i <= fdmax; i++) {
            print_log(log, "select: %d\n", i);
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
                                FD_CLR(ht->table[j]->sock, &master);
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
                        idx = add(key, &src_addr, cmsg_addr, new_sock, \
                                config_get_rate(key, &config), ht);
                        print_log(log, "added new socket %d\n", new_sock);
                    } 
                    /* conn exists - update timer and decrement counter */
                    else {
                        cur_entry = ht->table[idx];
                        cur_sock = cur_entry->sock;
                        cur_entry->last_use = time(NULL);
                        /* only send if ctr > 0 */
                        if (cur_entry->s_ctr >= 0) {
                            cur_entry->s_ctr -= recd;
                        }
                        /* counter < 0 */
                        else {
                            recd = 0;
                        }
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

                    cur_entry = get_entry_rev(i, &src_addr, ht);
                    cur_sock = cur_entry->sock;
                    /* get original source addr */
                    dst_addr = &(cur_entry->orig_src);
                    /* update time and counter */
                    cur_entry->last_use = time(NULL);
                    /* only send if ctr > 0 */
                    if (cur_entry->d_ctr > 0) {
                        cur_entry->d_ctr -= recd;
                    }
                    /* ctr < 0 */
                    else {
                        recd = 0;
                    }
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
                    print_log(log, "packet dropped\n");
                }
            } //end select case
        } //end select for loop
    } //end while(1)
}//end main

    /* Clean up on ctrl-c */
    void sighandler(int signum) {
        do_exit = 1;
    }

