#include "UDPProxy.h" 

#define INIT_TBL_SZ 997

char addr_buff[INET_ADDRSTRLEN];

/* convert 4-tuple connection to tuple_t struct 
 assumes long is 8 bytes, short 2 */ 
void addr_to_tuple(struct sockaddr_in *src, tuple_t *res) {
    res->src_ip = src->sin_addr.s_addr;
    res->src_port = src->sin_port;
//    res->dst_ip = dst->sin_addr.s_addr;
//    res->dst_port = dst->sin_port;
}

/* returns 1 if a and b represent the same connection (in either direction) 
 * a is usually being compared with b in the hashtable */
int compare_tuple(tuple_t *a, tuple_t *b) {
//    printf("comparing tuples.a->srcip:%lu a->src_port->%d b->src_ip:%lu b->src_port:%d\n", a->src_ip, a->src_port, b->src_ip, b->src_port);
    if ((a->src_ip == b->src_ip) && (a->src_port == b->src_port)) {
        return 1;
    }
    return 0;
}

/* copy tuple_t from src to dst */
void copy_tuple(tuple_t *src, tuple_t *dst) {
    dst->src_ip = src->src_ip;
    dst->src_port = src->src_port;
}

/* print an entry_t struct */
void print_entry(entry_t *e) {
    tuple_t *t = e->key;
    printf("Entry: Tuple: src: %lu:%u\n", t->src_ip, ntohs(t->src_port));
    inet_ntop(AF_INET, &t->src_ip, addr_buff, INET_ADDRSTRLEN);
    printf("src_ip string: %s\n", addr_buff);
    //TODO: ADD OTHER ENTRY INFO, INCLUDING ORIGINAL SOURCE, DST ADDR
}

/* print a hashtable */
void print_table(hashtable_t *ht) {
    printf("****HASHTABLE*****\n");
    int i;
    for (i = 0; i < ht->capacity; i++) {
        if (ht->table[i]) {
            printf("Entry at [%d] ", i);
            print_entry(ht->table[i]);
        }
    }
    printf("****END****\n");
}

/* create a new key/value pair */
entry_t *new_entry(tuple_t *key, struct sockaddr_in *orig_src, \
        struct sockaddr_in *orig_dst, int sock) {
    entry_t *new_entry = malloc(sizeof(entry_t));
    new_entry->key = malloc(sizeof(tuple_t));
    copy_tuple(key, new_entry->key);
    new_entry->last_use = time(NULL); //start timer
    new_entry->rate = DEFAULT_RATE;
    new_entry->s_ctr = TOKEN_MAX/2;
    new_entry->d_ctr = TOKEN_MAX/2;
    new_entry->orig_src.sin_family = AF_INET;
    new_entry->orig_src.sin_port = orig_src->sin_port;
    new_entry->orig_src.sin_addr.s_addr = orig_src->sin_addr.s_addr;
    new_entry->orig_dst.sin_family = AF_INET;
    new_entry->orig_dst.sin_port = orig_dst->sin_port;
    new_entry->orig_dst.sin_addr.s_addr = orig_dst->sin_addr.s_addr;
    new_entry->sock = sock;
    return new_entry;
}

/* destroy a key/value pair. */
void destroy_entry(entry_t *e) {
    free(e->key);
    close(e->sock);
    free(e);
    e = NULL; /* clear table entry */
}

/* create a new hashtable */
hashtable_t *new_ht() {
    hashtable_t *ht = malloc(sizeof(hashtable_t));
    ht->table = calloc(INIT_TBL_SZ, sizeof(entry_t *));
    ht->size = 0;
    ht->capacity = INIT_TBL_SZ;
    return ht;
}

/* destroy a hashtable */
void destroy_ht(hashtable_t *ht) {
    int i;
    for (i = 0; i < ht->capacity; i++) {
        if (ht->table[i]) {
            destroy_entry(ht->table[i]);
        }
    }
    free(ht->table);
    free(ht);
}

/* simple hash function inspired by
 * http://stackoverflow.com/questions/14409466/simple-hash-functions 
 * modified to use tuple_t instead of string */
uint hash(void *tuple_key, hashtable_t *ht) {
    uint8_t *key = (uint8_t *)tuple_key; 
    int i;
    uint hash = 0;
    for (i = 0; i < TUPLE_SZ; i++) {
        hash = key[i] + (hash << 6) + (hash << 16) - hash;
    }
    hash = hash % ht->capacity;
    return hash;
}

/* check if key, value is in table. Returns index on success. Returns -1
 * if not found */
int contains(tuple_t *key, hashtable_t *ht) {
    int hash_val = hash(key, ht);
    //not found
    if (!(ht->table[hash_val])) {
        return -1;
    } else { //entry is occupied
        //match
        if (compare_tuple(key, ht->table[hash_val]->key)) {
            return hash_val;
        }
        //no match - check following entries
        int entry, i = 0;
        while(++i < ht->size) {
            entry = ((hash_val + i) % ht->capacity);
            //found an unoccupied index
            if (!ht->table[entry]) {
                return -1;
            }
            //match
            else if (compare_tuple(key, ht->table[entry]->key)) {
                return entry;
            }
        }
    }
    return -1;
}

/* retrieve an entry from a hashtable. Returns entry on success, -1 on failure */
entry_t *get(tuple_t *key, hashtable_t *ht) {
    int index = contains(key, ht);
    //there is something at the hash index
    if (index) {
        //match
        if (compare_tuple(key, ht->table[index]->key)) {
            return ht->table[index];
        } 
        //no match - check following entries
        index = hash(key, ht);
        int entry, i = 0;
        while(++i < ht->size) {
            entry = (index + i) % ht->capacity;
            //found an unoccupied index
            if (!ht->table[entry]) {
                return (entry_t *)-1;
            }
            //match
            if (compare_tuple(key, ht->table[entry]->key)) {
                return ht->table[entry];
            }
        }
    }
    //nothing at the hash index
    return (entry_t *)-1;
}

/* remove an entry from the hashtable */
void remove_entry(tuple_t *key, hashtable_t *ht) {
    int index = contains(key, ht);
    if (index != -1) {
        destroy_entry(ht->table[index]);
        ht->table[index] = 0;
    }
}

/* add an entry to the hash table. Returns the index of the entry 
 * if an entry already exists, returns the index */
int add(tuple_t *key, struct sockaddr_in *orig_src, \
        struct sockaddr_in *orig_dst, int sock, hashtable_t *ht) {
    int hash_val = hash(key, ht);
    int idx = hash_val;
    entry_t *entry = new_entry(key, orig_src, orig_dst, sock);
    print_entry(entry);
    //location is occupied
    if (ht->table[hash_val]) {
        int i;
        for(i = 0; i < ht->capacity; i++) {
            idx = (hash_val + i) % ht->capacity;
            //found an unoccupied container
            if(!ht->table[idx]) {
                hash_val = idx;
                break;
            } 
            //entry is already present
            else if (compare_tuple(key, ht->table[idx]->key)) {
                destroy_entry(entry);
                return idx;
            }
        }
    }
    ht->table[hash_val] = entry;
    ht->size++;
    return idx; 
}

/*
int main(int argc, char **argv) {
    hashtable_t *ht = new_ht();
    struct sockaddr_in addr; 
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4444);
    addr.sin_addr.s_addr = htonl(2130706433);
    tuple_t *key = calloc(1, sizeof(tuple_t));
    addr_to_tuple(&addr, &addr, key);
    add(key, 1, ht);
    print_table(ht);
    memset((char *) &addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(4443);
    addr.sin_addr.s_addr = htonl(2130706433);
    addr_to_tuple(&addr, &addr, key);
    add(key, 2, ht);
    print_table(ht);
    remove_entry(key, ht);
    print_table(ht);
    free(key);
    destroy_ht(ht);
} */
