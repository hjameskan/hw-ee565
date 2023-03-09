#ifndef HASHMAP_H
#define HASHMAP_H

#define HASH_TABLE_SIZE 256

struct Entry {
    char *key;
    char *value;
    struct Entry *next;
};

struct HashMap {
    struct Entry *entries[HASH_TABLE_SIZE];
};

// A simple hash function that takes a string and returns an index
unsigned int hash(const char *str);

// Allocates a new hash map
struct HashMap *hashmap_new();

// Frees a hash map and all its entries
void hashmap_free(struct HashMap *map);

// Adds a key-value pair to the hash map
void hashmap_put(struct HashMap *map, const char *key, char *value);

// Gets the value associated with a key, or returns NULL if not found
char* hashmap_get(struct HashMap *map, const char *key);

void hashmap_print_all(struct HashMap *map);

#endif
