#include "UDPProxy.h" 

#define INIT_TBL_SZ 37

char addr_buff[INET_ADDRSTRLEN];


/* a key/value pair in the hashtable */
typedef struct entry {
    tuple_t *key;
    int value;
} entry_t;

/* the hashtable */
typedef struct hashtable {
    int size;
    int capacity;
    entry_t **table;
} hashtable_t;


void print_entry(entry_t *e) {
    tuple_t *t = e->key;
    printf("Entry: Tuple: src: %u:%u dst: %u:%u. Value: %d\n", t->src_ip, \
            ntohs(t->src_port), t->dst_ip, ntohs(t->dst_port), e->value);
    inet_ntop(AF_INET, &t->src_ip, addr_buff, INET_ADDRSTRLEN);
    printf("src_ip string: %s\n", addr_buff);
    inet_ntop(AF_INET, &t->dst_ip, addr_buff, INET_ADDRSTRLEN);
    printf("dst_ip string: %s\n", addr_buff);
}

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
    printf("new entry: size: %d\n", sizeof(entry_t));
    entry_t *new_entry = malloc(sizeof(entry_t));
    new_entry->key = malloc(sizeof(tuple_t));
    copy_tuple(key, new_entry->key);
    new_entry->value = value;
    return new_entry;
}

/* destroy a key/value pair. */
void destroy_entry(entry_t *e) {
    printf("destroy_entry\n");
    free(e->key);
    printf("key freed\n");
    free(e);
    printf("entry freed\n");
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
    printf("destroy_ht\n");
    int i;
    for (i = 0; i < ht->capacity; i++) {
        if (ht->table[i]) {
            printf("about to destroy:");
            print_entry(ht->table[i]);
            destroy_entry(ht->table[i]);
        }
    }
    printf("freeing table\n");
    free(ht->table);
    free(ht);
    printf("hashtable destroyed\n");
}

///* simple hash function inspired by
// * http://stackoverflow.com/questions/14409466/simple-hash-functions */
//uint hash(char *key, hashtable_t *ht) {
//    int i;
//    uint hash = 0;
//    for (i = 0; key[i]!='\0'; i++) {
//        hash = key[i] + (hash << 6) + (hash << 16) - hash;
//    }
//    hash = hash % ht->capacity;
//    return hash;
//}

/* simple hash function inspired by
 * http://stackoverflow.com/questions/14409466/simple-hash-functions */
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

/* Check if table is above 70% occupancy: if so, increase size 2X */
//void check_capacity(hashtable_t *ht) {
//    printf("in check_capacity. Capacity:%d\n, size:%d\n", ht->capacity, ht->size);
//    /* table is almost full = increase size by 2X */
//    if (((float)ht->capacity / (float)ht->size) > .7) {
//        entry_t **tbl = calloc(ht->capacity * 2, sizeof(entry_t));
//        int i;
//        for (i = 0; i < ht->capacity; i++) {
//            tbl[i] = ht->table[i];
//        }
//        free(ht->table);
//        ht->table = tbl;
//        ht->size *= 2;
//    }
//}

/* check if key, value is in table. Returns index on success. Returns 0
 * if not found */
int contains(tuple_t *key, hashtable_t *ht) {
    int hash_val = hash(key, ht);
    //not found
    if (!(ht->table[hash_val])) {
        return 0;
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
                return 0;
            }
            //match
            else if (compare_tuple(key, ht->table[entry]->key)) {
                return entry;
            }
        }
    }
    return 0;
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

void remove_entry(tuple_t *key, hashtable_t *ht) {
    int index = contains(key, ht);
    if (index) {
        destroy_entry(ht->table[index]);
        ht->table[index] = 0;
    }
}

/* add an entry to the hash table. Returns the index of the entry 
 * if an entry already exists, returns the index */
int add(tuple_t *key, int value, hashtable_t *ht) {
    int idx, hash_val = hash(key, ht);
    entry_t *entry = new_entry(key, value);
    //location is occupied
    if (ht->table[hash_val]) {
        int i;
        for(i = 0; i < ht->size; i++) {
            idx = (hash_val + i) % ht->capacity;
            //found an unoccupied container
            if(!ht->table[idx]) {
                hash_val = idx;
            } 
            //entry is already present
            else if (compare_tuple(key, ht->table[idx]->key)) {
                destroy_entry(entry);
                return idx;
            }
        }
    }
    ht->table[hash_val] = entry;
    //printf("added to index: key:%s\tvalue:%d\n", \
    //        ht->table[hash_val]->key, ht->table[hash_val]->value);
    return idx; 
}


int main(int argc, char **argv) {
    hashtable_t *ht = new_ht();
    printf("created hashtable\n");
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
} 
