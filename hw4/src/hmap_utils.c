#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>


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
unsigned int hash_func(void *key, int size, int size_of_key) {
    if(size_of_key == NULL){
        size_of_key = sizeof(key);
    }
    unsigned int hash = 0;
    char *p = (char*)key;
    // for (int i = 0; i < sizeof(key); i++) {
    for (int i = 0; i < size_of_key; i++) {
        hash = hash * 31 + *(p + i);
    }
    return hash % size;
}

// Creates a new hash table with the specified size
hash_table* hash_table_create(int size) {
    hash_table *ht = malloc(sizeof(hash_table));
    ht->size = size;
    ht->buckets = malloc(sizeof(hash_node*) * size);
    for (int i = 0; i < size; i++) {
        ht->buckets[i] = NULL;
    }
    return ht;
}

// Inserts a key-value pair into the hash table
void hash_table_put(hash_table *ht, void *key, void *value, int size) {
    if(size == NULL){
        size = sizeof(key);
    }
    unsigned int hash = hash_func(key, ht->size, size);
    // printf("01key: %s \n", (char*)key);
    // printf("hash: %d \n", hash);
    hash_node *node = ht->buckets[hash];
    while (node != NULL) {
        if (node->key == key || memcmp(node->key, key, size) == 0) {
            // Key already exists, update value
            // printf("02Key: %s already exists, update value key \n", (char*)key);
            node->value = value;
            return;
        }
        node = node->next;
    }
    // Key not found, insert new node at the head of the linked list
    // printf("03Key: %s NOT FOUND \n", (char*)key);
    // fflush(stdout);
    node = malloc(sizeof(hash_node));
    // char *key_copy = malloc(sizeof(key));
    // memcpy(key_copy, key, size);
    // node->key = key_copy;
    node->key = key;
    node->value = value;
    node->next = ht->buckets[hash];
    ht->buckets[hash] = node;
}

void hash_table_put_str(hash_table *ht, void *key, void *value, int size) {
    if(size == NULL){
        size = sizeof(key);
    }
    unsigned int hash = hash_func(key, ht->size, size);
    hash_node *node = ht->buckets[hash];
    while (node != NULL) {
        if (node->key == key || memcmp(node->key, key, size) == 0) {
            node->value = value;
            return;
        }
        node = node->next;
    }
    char *key_copy = malloc(256);
    memcpy(key_copy, key, size);
    node = malloc(sizeof(hash_node));
    node->key = key_copy;
    node->value = value;
    node->next = ht->buckets[hash];
    ht->buckets[hash] = node;
}

// Gets the value associated with the given key
void* hash_table_get(hash_table *ht, void *key, int size) {
    if(size == NULL){
        size = sizeof(key);
    }
    unsigned int hash = hash_func(key, ht->size, size);
    hash_node *node = ht->buckets[hash];
    while (node != NULL) {
        if (node->key == key || memcmp(node->key, key, size) == 0) {
            return node->value;
        }
        node = node->next;
    }
    // Key not found
    return NULL;
}

// Deletes the node with the given key from the hash table
void hash_table_delete(hash_table *ht, void *key, int size) {
    if(size == NULL){
        size = sizeof(key);
    }
    unsigned int hash = hash_func(key, ht->size, size);
    hash_node *node = ht->buckets[hash];
    hash_node *prev = NULL;
    while (node != NULL) {
        if (node->key == key || memcmp(node->key, key, size) == 0) {
            if (prev == NULL) {
                ht->buckets[hash] = node->next;
            } else {
                prev->next = node->next;
            }
            free(node);
            return;
        }
        prev = node;
        node = node->next;
    }
}

// Deallocates the hash table and all of its nodes
void hash_table_destroy(hash_table *ht) {
    for (int i = 0; i < ht->size; i++) {
        hash_node *node = ht->buckets[i];
        while (node != NULL) {
            hash_node *temp = node;
            node = node->next;
            free(temp);
        }
    }
    free(ht->buckets);
    free(ht);
}

void print_keys(hash_table *ht) {
    for (int i = 0; i < ht->size; i++) {
        hash_node *node = ht->buckets[i];
        while (node != NULL) {
            printf("PRINT KEYS HERE: %s\n", (char*)node->key);
            fflush(stdout);
            node = node->next;
        }
    }
}
