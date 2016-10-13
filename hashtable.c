#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INIT_TBL_SZ 37

/* a key/value pair in the hashtable */
typedef struct entry {
    char *key;
    int value;
} entry_t;

/* the hashtable */
typedef struct hashtable {
    int size;
    int capacity;
    entry_t **table;
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

/* destroy a key/value pair.
 * */
void destroy_entry(entry_t *e) {
    free(e->key);
    free(e);
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
    free(ht);
}

/* simple hash function inspired by
 * http://stackoverflow.com/questions/14409466/simple-hash-functions */
uint hash(char *key, hashtable_t *ht) {
    int i;
    uint hash = 0;
    for (i = 0; key[i]!='\0'; i++) {
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
int contains(char *key, hashtable_t *ht) {
    int hash_val = hash(key, ht);
    //not found
    if (!(ht->table[hash_val])) {
        return 0;
    } else { //entry is occupied
        //match
        if (*(ht->table[hash_val]->key) == *key) {
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
            else if (*(ht->table[entry]->key) == *key) {
                return entry;
            }
        }
    }
    return 0;
}

/* retrieve an entry from a hashtable. Returns value on success, -1 on failure */
int get(char *key, hashtable_t *ht) {
    int index = contains(key, ht);
    //there is something at the hash index
    if (index) {
        //match
        if (*(ht->table[index]->key) == *key) {
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
            if (*(ht->table[entry]->key) == *key) {
                return entry;
            }
        }
    }
    //nothing at the hash index
    return -1;
}

void remove_entry(char *key, hashtable_t *ht) {
    int index = contains(key, ht);
    if (index) {
        destroy_entry(ht->table[index]);
    }
}

/* add an entry to the hash table. Returns the index of the entry 
 * if an entry already exists, returns the index */
int add(char *key, int value, hashtable_t *ht) {
    int idx, len = strlen(key);
    int hash_val = hash(key, ht);
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
            else if (strncmp(key, ht->table[idx]->key, len) == 0) {
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

void print_table(hashtable_t *ht) {
    printf("****HASHTABLE*****\n");
    int i;
    for (i = 0; i < ht->capacity; i++) {
        if (ht->table[i]) {
            printf("*****[%d]:\tkey:%s\tvalue:%d\n", i, ht->table[i]->key, \
                    ht->table[i]->value);
        }
    }
}

/* int main(int argc, char **argv) {
    hashtable_t *ht = new_ht();
    printf("created hashtable\n");
    add("one", 1, ht);
    add("two", 2, ht);
    add("three", 3, ht);
    print_table(ht);
    printf("Contains(\"two\", ht):%d\n", contains("two", ht));
    remove_entry("two", ht);
    printf("after remove: Contains(\"two\", ht):%d\n", contains("two", ht));
    add("two", 2, ht);
    printf("after 2nd add: Contains(\"two\", ht):%d\n", contains("two", ht));
    int three = get("three", ht);
    printf("got three:%d\n", three);
    add("ten", 10, ht);
    add("ten", 10, ht);
    print_table(ht);
    destroy_ht(ht);
} */
