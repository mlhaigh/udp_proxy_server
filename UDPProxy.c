#include "UDPProxy.h"

#define PORT 7777
#define REV_PORT 8888

tuple_t *key; 
hashtable_t *ht;
volatile int do_exit = 0;
void sighandler(int);

/* return 1 if original direction, 0 if reverse */
int check_direction(struct sockaddr_in *src, struct sockaddr_in *dst) {
    printf("Checking direction\n");
    if (ntohs(dst->sin_port) == PORT) {
        printf("reverse\n");
        return 0;
    }
    printf("original\n");
    return 1;
}

/* add packet to buffer. drop packet if buffer is full */
void buf_pkt (char *data, r_buf_t *r_buf, int len) {
    printf("buffering packet\n");
    /* there is enough room to buffer the new packet */
    if ((R_BUF_SZ - r_buf->buf_sz) >= len) {
        printf("room in buffer\n");
        /* buffer will wrap around */
        if ((r_buf->buf_start + len) > R_BUF_SZ) {
            printf("buffer will wrap around\n");
            /* write to end of buffer */
            memcpy(&(r_buf->buf[r_buf->buf_start]), data, \
                    (R_BUF_SZ - r_buf->buf_start));
            printf("wrote to end\n");
            /* write from beginning of buffer */
            memcpy(r_buf->buf, &(data[(R_BUF_SZ - r_buf->buf_start)]), \
                    (len - R_BUF_SZ - r_buf->buf_start));
            printf("wrote to beginninh\n");
            /* update buf stats */
            r_buf->buf_sz += len;
            r_buf->buf_end = (len - R_BUF_SZ - r_buf->buf_start);
            printf("updated stats\n");
        }
        /* no wrap around */
        else {
            printf("no wrap around\n");
            memcpy(&(r_buf->buf[r_buf->buf_start]), data, len);
            printf("wrote packet\n");
            /* update buf stats */
            r_buf->buf_sz += len;
            r_buf->buf_end += len;
            printf("updated stats\n");
        }
    }
}

/* get buffered packet for connection. Writes to buf. Returns the number of 
 * bytes retreived */
int get_bufd_pkt(r_buf_t *r_buf, char *buf, int len) {
    printf("getting buffered packet\n");
    int _len;
    /* something is buffered */
    if (r_buf->buf_sz > 0) {
        printf("something is buffered\n");
        _len = (r_buf->buf_sz < len) ? r_buf->buf_sz : len;
        /* packet wraps around in buffer */
        if ((R_BUF_SZ - r_buf->buf_start) < _len) {
            printf("packet wraps around\n");
            /* read until end of buffer */
            memcpy(buf, &(r_buf->buf[r_buf->buf_start]), \
                    (R_BUF_SZ - r_buf->buf_start));
            printf("read at end\n");
            /* read from start of buffer */
            memcpy(&(buf[R_BUF_SZ - r_buf->buf_start]), r_buf->buf, \
                    (_len - (R_BUF_SZ - r_buf->buf_start)));
            printf("read from start\n");
            /* update buf stats */
            r_buf->buf_sz -= _len;
            r_buf->buf_start = (_len - (R_BUF_SZ - r_buf->buf_start));
            printf("updated stats\n");
        }
        else {
            printf("packet does not wrap around\n");
            /* packet does not wrap around in buffer */
            memcpy(buf, &(r_buf->buf[r_buf->buf_start]), _len);
            printf("read packet\n");
            /* update buf stats */
            r_buf->buf_sz -= _len;
            r_buf->buf_end -= _len;
            printf("updated stats\n");
        }
        return _len;
    }
    return 0;
}

void generate_token(hashtable_t *ht, char *buf, double elapsed, int sock) {
    printf("generating token\n");
    printf("elapsed: %f\n", elapsed);
    if (ht->size > 0) {
        int i, len = 0;
        entry_t *cur;
        struct sockaddr_in *dst;
        char addr_buff[INET_ADDRSTRLEN];
        for (i = 0; i < ht->capacity; i++) {
            if (ht->table[i]) {
                printf("found entry at %d\n", i);
                cur = ht->table[i];
                printf("cur->rate: %f\n", cur->rate);
                printf("cur->s_ctr + elapsed * ((cur->rate)/2):%f\n", \
                        cur->s_ctr + elapsed * ((cur->rate)/2));
                printf("cur->d_ctr + elapsed * ((cur->rate)/2):%f\n", \
                        cur->d_ctr + elapsed * ((cur->rate)/2));
                /* increment counters */
                printf("counters before update: s_ctr:%d. d_ctr:%d\n", \
                        cur->s_ctr, cur->d_ctr);
                cur->s_ctr = MIN((cur->s_ctr + elapsed * ((cur->rate)/2)), \
                        (TOKEN_MAX/2));
                cur->d_ctr = MIN((cur->d_ctr + elapsed * ((cur->rate)/2)), \
                        (TOKEN_MAX/2));
                printf("updated counters. s_ctr:%d. d_ctr:%d\n", cur->s_ctr, \
                        cur->d_ctr);

                /* send a packet if a buffered connection became available. 
                * send from larger buffer */
                if (cur->src_buf.buf_sz > 0 || cur->dst_buf.buf_sz > 0) {
                    printf("buffered packets are waiting to be sent\n");
                    /* sender buffer is larger */
                    if (cur->src_buf.buf_sz > cur->dst_buf.buf_sz) {
                        printf("sender buffer is larger\n");
                        printf("new packet ready to be sent from sender\n");
                        len = get_bufd_pkt(&cur->src_buf, buf, PKT_SZ);
                        dst = &cur->orig_dst;
                    }
                    /* dest buffer is larger */
                    else {
                        printf("dest buffer is larger\n");
                        printf("new packet ready to be sent from dest\n");
                        len = get_bufd_pkt(&cur->dst_buf, buf, PKT_SZ);
                        dst = &cur->orig_src;
                    }

                }

                /* send packet that is now ready */
                if (len > 0) {
                    if (sendto(sock, buf, 1400, 0, \
                            (struct sockaddr *)dst, len) < 0) {
                        die("Send Error");
                    }
                    inet_ntop(AF_INET, &dst->sin_addr.s_addr, addr_buff, \
                            INET_ADDRSTRLEN);
                    printf("Sent buffered packet to: %s:%d on fd %d\n", addr_buff, \
                            ntohs(dst->sin_port), sock);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    struct sockaddr_in addr, src_addr, *cmsg_addr, *dst_addr;
    int sock, idx, fdmax, i, j, timer_fd; 
    int res, recd, dir, len = sizeof(addr);
    char buff[BUFF_LEN];
    char addr_buff[INET_ADDRSTRLEN];
    fd_set master, readfds;
    struct itimerspec timer;
    ssize_t timer_read;
    uint64_t  exp;
    struct timeval last, now;
    double elapsed;
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

    /* add listener socket to the master set */
    sock = bind_sock(INADDR_ANY, PORT); //forward direction
    fcntl(sock, F_SETFL, O_NONBLOCK);
    FD_SET(sock, &master);
    printf("listener port set to fd %d\n", sock);
    /* keep track of the biggest file descriptor */
    fdmax = sock;

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

        generate_token(ht, buff, elapsed, sock);

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
                                destroy_entry(ht->table[j]);
                                ht->table[j] = 0;
                            }
                        }
                    }
                }
                /* iptables redirected packet - may be new or not */
                else if (i == sock) {
                    printf("sock\n");
                    recd = recvmsg(sock, &msg, 0);
                    if (recd < 0) {
                        die("Receive error");
                    }
                    printf("packet read: len %d\n", recd);

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

                    /* Ignore broadcast due to permission error */
                    if(((cmsg_addr->sin_addr.s_addr >> 24) & 255) == 255) {
                        printf("broadcast packet\n");
                        recd = 0;
                        break;
                    }

                    /* determine if this is original or reverse direction 
                     * dir == 1 if original, 0 if reverse */
                    dir = check_direction(&src_addr, cmsg_addr);

                    /* original direction */
                    if(dir == 1) {
                        dst_addr = cmsg_addr;
                        /* check for socket in table */
                        printf("finding original direction in table\n");
                        addr_to_tuple(cmsg_addr, key);
                        idx = contains(key, ht);
                        printf("contains: %d\n", idx);
                        /* new connection: create entry */
                        if (idx == -1) {
                            printf("new connection. adding\n");
                            idx = add(key, &src_addr, cmsg_addr, ht);
                            printf("added at %d. printing table\n", idx);
                            print_table(ht);
                        } 
                        /* conn exists - update timer and decrement counter */
                        else {
                            printf("connection exists\n");
                            ht->table[idx]->last_use = time(NULL);
                            ht->table[idx]->s_ctr -= recd;
                            printf("updated s_ctr to %d\n", \
                                    ht->table[idx]->s_ctr);
                            //TODO: COUNTER NEVER CHECKED FOR ZERO SINCE PACKET IS ALREADY BUFFERED
                        }
                    }

                    /* previously established connection coming in return direction */
                    else if (dir == 0) {

                        /* check for socket in table */
                        printf("finding rev direction in table\n");
                        addr_to_tuple(&src_addr, key);
                        idx = contains(key, ht);
                        printf("contains: %d\n", idx);
                        if (idx < 0) {
                            die("rev direction could not find entry");
                        }
                        /* get original source addr */
                        dst_addr = &(ht->table[idx]->orig_src);
                        /* update time and counter */
                        ht->table[idx]->last_use = time(NULL);
                        printf("d_ctr before update:%d\n", ht->table[idx]->d_ctr);
                        printf("subtracting recd:%d\n", recd);
                        ht->table[idx]->d_ctr -= recd;
                        printf("updated d_ctr to %d\n", ht->table[idx]->d_ctr);
                        //TODO: BUFFER SINCE CANNOT RECEIVE
                    }

                }
                /* if data was received, send */
                if (recd > 0) {
                    if (sendto(sock, buff, 1400, 0, \
                                (struct sockaddr *)dst_addr, len) < 0) {
                        die("Send Error");
                    }
                    inet_ntop(AF_INET, &dst_addr->sin_addr.s_addr, addr_buff, \
                            INET_ADDRSTRLEN);
                    printf("Sent packet to: %s:%d on fd %d\n", addr_buff, \
                            ntohs(dst_addr->sin_port), sock);
                }
            }
        } //end select for loop
    }
}

/* Clean up on ctrl-c */
void sighandler(int signum) {
    do_exit = 1;
}

