#include "UDPProxy.h" 

#define INIT_TBL_SZ 37

char addr_buff[INET_ADDRSTRLEN];

/* convert 4-tuple connection to tuple_t struct 
 assumes long is 8 bytes, short 2 
 presently destination address is hard-coded */
void addr_to_tuple(struct sockaddr_in *src, struct sockaddr_in *dst, \
        tuple_t *res) {
    struct in_addr *dst_addr = malloc(sizeof(struct in_addr));
    res->src_ip = src->sin_addr.s_addr;
    res->src_port = src->sin_port;
    inet_pton(AF_INET, DST_IP, dst_addr);
    res->dst_ip = dst_addr->s_addr;
    res->dst_port = htons(DST_PORT);
    free(dst_addr);
}

/* returns 1 if a and b represent the same connection (in either direction) */
int compare_tuple(tuple_t *a, tuple_t *b) {
    if (((a->src_ip == b->src_ip) && (a->src_port == b->src_port) && \
        (a->dst_ip == b->dst_ip) && (a->dst_port == b->dst_port)) || \
        ((a->src_ip == b->dst_ip) && (a->src_port == b->dst_port) && \
         (a->dst_ip == b->src_ip) && (a->dst_port == b->src_port))) {
        return 1;
    }
    return 0;
}

/* copy tuple_t from src to dst */
void copy_tuple(tuple_t *src, tuple_t *dst) {
    dst->src_ip = src->src_ip;
    dst->src_port = src->src_port;
    dst->dst_ip = src->dst_ip;
    dst->dst_port = src->dst_port;
}

/* print an entry_t struct */
void print_entry(entry_t *e) {
    tuple_t *t = e->key;
    printf("Entry: Tuple: src: %lu:%u dst: %lu:%u. Value: %d\n", t->src_ip, \
            ntohs(t->src_port), t->dst_ip, ntohs(t->dst_port), e->value);
    inet_ntop(AF_INET, &t->src_ip, addr_buff, INET_ADDRSTRLEN);
    printf("src_ip string: %s\n", addr_buff);
    inet_ntop(AF_INET, &t->dst_ip, addr_buff, INET_ADDRSTRLEN);
    printf("dst_ip string: %s\n", addr_buff);
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
entry_t *new_entry(tuple_t *key, int value) {
    entry_t *new_entry = malloc(sizeof(entry_t));
    new_entry->key = malloc(sizeof(tuple_t));
    copy_tuple(key, new_entry->key);
    new_entry->value = value;
    new_entry->last_use = time(NULL); //start timer
    new_entry->rate = DEFAULT_RATE;
    new_entry->counter = TOKEN_MAX; 
    return new_entry;
}

/* destroy a key/value pair. */
void destroy_entry(entry_t *e) {
    close(e->value); /* close socket */
    free(e->key);
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

/* retrieve an entry from a hashtable. Returns value on success, -1 on failure */
int get(tuple_t *key, hashtable_t *ht) {
    int index = contains(key, ht);
    //there is something at the hash index
    if (index) {
        //match
        if (compare_tuple(key, ht->table[index]->key)) {
            return ht->table[index]->value;
        } 
        //no match - check following entries
        index = hash(key, ht);
        int entry, i = 0;
        while(++i < ht->size) {
            entry = (index + i) % ht->capacity;
            //found an unoccupied index
            if (!ht->table[entry]) {
                return -1;
            }
            //match
            if (compare_tuple(key, ht->table[entry]->key)) {
                return entry;
            }
        }
    }
    //nothing at the hash index
    return -1;
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
int add(tuple_t *key, int value, hashtable_t *ht) {
    int hash_val = hash(key, ht);
    int idx = hash_val;
    entry_t *entry = new_entry(key, value);
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
