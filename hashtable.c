#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_TBL_SZ 32

/* a key/value pair in the hashtable */
typedef struct entry {
    char *key;
    int value;
} entry_t;

/* the hashtable */
typedef struct hashtable {
    entry_t *table[];
    int size;
    int capacity;
} hashtable_t;

/* create a new key/value pair */
entry_t *new_entry(char *key, int value) {
    entry_t *new_entry = malloc(sizeof(entry_t));
    int len = strlen(key);
    new_entry->key = malloc(len * sizeof(char));
    strncpy(new_entry->key, key, len);
    new_entry->value = value;
    return new_entry;
}

/* destroy a key/value pair. :w
 * */
void destroy_entry(entry_t *e) {
    free(e->key);
    free(e);
}

/* create a new hashtable */
hashtable_t *new_ht() {
    hashtable_t *ht = malloc(sizeof(hashtable_t));
    entry *tbl[INIT_TBL_SZ] = calloc(INIT_TBL_SZ, sizeof(entry));
    ht.table = tbl;
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
    free(ht);
}

/* simple hash function inspired by
 * http://stackoverflow.com/questions/14409466/simple-hash-functions */
int hash(char *key, hashtable_t *ht) {
    int i, hash = 0;
    for (i = 0; key[i]!='\0'; i++) {
        hash = key[i] + (hash << 6) + (hash << 16) - hash;
    }
    hash = hash % ht->capacity;
}

void check_capacity(hashtable_t *ht) {
    /* table is almost full = increase size by 2x */
    if (((float)ht->capacity / (float)ht->size) > .7) {
        entry *tbl[ht->capacity * 2] = calloc(ht->capacity * 2, sizeof(entry));
        int i;
        for (i = 0; i < ht->capacity; i++) {
            tbl[i] = ht->table[i];
        }
        free(ht->table);
        ht->table = tbl;
        ht->size *= 2;
    }
}

/* check if key, value is in table. Returns index on success. Returns 0
 * if not found */
int contains(char *key, hashtable_t *ht) {
    int hash = hash(key);
    //not found
    if (!ht->table[hash]) {
        return 0;
    } else { //entry is occupied
        //match
        if (*(ht->table[hash].key) == *key) {
            return hash;
        }
        //no match - check following entries
        int entry, i = 0;
        while(++i < ht->size) {
            entry = (hash + i) % ht->capacity;
            //found an unoccupied index
            if (!ht->table[entry]) {
                return 0;
            }
            //match
            if (*(ht->table[entry].key) == *key) {
                return entry;
            }
        }
    }
    return 0;
}

/* retrieve an entry from a hashtable. Returns value on success, -1 on failure */
int get(char *key, Hashtable *ht) {
    int index = contains(key, ht);
    //there is something at the hash index
    if (index) {
        //match
        if (*(ht->table[index].key) == *key) {
            return ht->table[index].value;
        } 
        //no match - check following entries
        int entry, i = 0;
        while(++i < ht->size) {
            entry = (hash + i) % ht->capacity;
            //found an unoccupied index
            if (!ht->table[entry]) {
                return -1;
            }
            //match
            if (*(ht->table[entry].key) == *key) {
                return entry;
            }
        }
    }
    //nothing at the hash index
    return -1;
}

void remove(char *key, hashtable_t *ht) {
    int index = contains(key, ht);
    if (index) {
        destroy_entry(ht->table[index]);
    }
}

/* add an entry to the hash table. Returns the index of the entry 
 * returns -1 on failure. Doubles size of hash table if near full */
int add(char *key, int value, hashtable_t *ht) {
    check_capacity(ht);
    int hash = contains(key, ht);
    //index is occupied
    if (hash) {
        //match
        if (*(ht->table[hash].key) == *key) {
            return hash;
        } else { //collision
            int idx, i = 0;
            while(++i < ht->size) {
                idx = (hash + i) % ht->capacity;
                //found an unoccupied index
                if(!ht->table[idx]) {
                    entry_t *new_entry = new_entry(key, value);
                    ht->table[entry] = new_entry;
                    return idx; 
                }
            }
        }
    } 
    return -1;
}

int main(int argc, char **argv) {
    hashtable_t *ht = new_ht();
    add("one", 1, ht);
    add("two", 2, ht);
    add("three", 3, ht);
    printf("Contains(\"two\", ht):%d", contains("two", ht));

}
