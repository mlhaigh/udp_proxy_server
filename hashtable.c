#include "UDPProxy.h"

#define INIT_TBL_SZ 32

typedef struct hashtable {
    int *table[];
    int size;
    int capacity
} Hashtable;

Hashtable new_hashtable() {
    Hashtable ht;
    int *tbl[INIT_TBL_SZ] = calloc(INIT_TBL_SZ, sizeof(int));
    ht.table = tbl;
    ht->size = 0;
    ht->capacity = INIT_TBL_SZ;
    return ht;
}

/* simple hash function inspired by
 * http://stackoverflow.com/questions/14409466/simple-hash-functions */
int hash(char *key, Hashtable *ht) {
    int i, hash = 0;
    for (i = 0; key[i]!='\0'; i++) {
        hash = key[i] + (hash << 6) + (hash << 16) - hash;
    }
    hash = hash % ht->capacity;
}

void check_capacity(Hashtable *ht) {
    /* table is almost full = increase size by 2x */
    if (float)ht->capacity / (float)ht->size > .7 {
        int *tbl[ht->capacity * 2] = calloc(ht->capacity * 2, sizeof(int));
        int i;
        for (i = 0; i < ht->capacity; i++) {
            tbl[i] = ht->table[i];
        }
        free(ht->table);
        ht->table = tbl;
        ht->size *= 2;
    }
}

int add(char *key, Hashtable *ht) {
    int hash = hash(key);
    //index is occupied
    if (ht->table[hash]) {
        check_capacity(ht);
        //match
        if (ht->table[hash] == *key) {
            //this key is already in the table
        } else { //collision
            int i;
            while 
        }
    }
}
