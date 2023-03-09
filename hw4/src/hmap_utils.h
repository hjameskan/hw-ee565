#ifndef HASH_TABLE_H
#define HASH_TABLE_H

#include <stdlib.h>
#include <string.h>

// The linked list node struct
typedef struct hash_node {
    void *key;
    void *value;
    struct hash_node *next;
} hash_node;

// The hash table struct
typedef struct hash_table {
    int size;
    hash_node **buckets;
} hash_table;

// The hashing function, takes a key and the size of the hash table
unsigned int hash_func(void *key, int size, int size_of_key);
// unsigned int hash_func(void *key, int size);

// Creates a new hash table with the specified size
hash_table* hash_table_create(int size);

// Inserts a key-value pair into the hash table
void hash_table_put(hash_table *ht, void *key, void *value, int size);
void hash_table_put_str(hash_table *ht, void *key, void *value, int size);

// Gets the value associated with the given key
void* hash_table_get(hash_table *ht, void *key, int size);

// Deletes the node with the given key from the hash table
void hash_table_delete(hash_table *ht, void *key, int size);

// Deallocates the hash table and all of its nodes
void hash_table_destroy(hash_table *ht);

void print_keys(hash_table *ht);

#endif /* HASH_TABLE_H */
