#ifndef UDPPROXY_H
#define UDPPROXY_H

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
#include <signal.h>
#include <sys/timerfd.h>
#include <errno.h>

#define BUFF_LEN 1500
//#define DST_PORT 8889
//#define DST_IP "127.0.0.1"
#define TUPLE_SZ 20
#define TIME_OUT 300 /* How many second to terminate connection */
#define TOKEN_MAX 10000 /* maximum rate build-up (in MB) */
#define DEFAULT_RATE 10 /* MB per sec */
#define CMSG_ARR_LEN 64
#define S_BUF_SZ 10000

/* from http://stackoverflow.com/questions/3437404/min-and-max-in-c */
#define MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

/* a 4-tuple representing a UDP connection */
typedef struct tuple {
    unsigned long src_ip;
    unsigned short src_port;
    unsigned long dst_ip;
    unsigned short dst_port;
} tuple_t;

/* a key/value pair in the hashtable */
typedef struct entry {
    tuple_t *key;
    int value;
    time_t last_use;
    double rate;
    int counter;
    char *s_buf;
} entry_t;

/* a hashtable */
typedef struct hashtable {
    int size;
    int capacity;
    entry_t **table;
} hashtable_t;

void print_entry(entry_t *e);
void print_table(hashtable_t *ht);
entry_t *new_entry(tuple_t *key, int value);
void destroy_entry(entry_t *e);
hashtable_t *new_ht();
void destroy_ht(hashtable_t *ht);
uint hash(void *tuple_key, hashtable_t *ht);
int contains(tuple_t *key, hashtable_t *ht);
int get(tuple_t *key, hashtable_t *ht);
void remove_entry(tuple_t *key, hashtable_t *ht);
int add(tuple_t *key, int value, hashtable_t *ht);
void addr_to_tuple(struct sockaddr_in *src, struct sockaddr_in *dst, \
        tuple_t *res);
int compare_tuple(tuple_t *a, tuple_t *b);
void copy_tuple(tuple_t *src, tuple_t *dst);
void die(char *msg);
int bind_sock(int ip, int port);

#endif
