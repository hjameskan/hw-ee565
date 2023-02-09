#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashmap.h"

#define HASH_TABLE_SIZE 256

// A simple hash function that takes a string and returns an index
unsigned int hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    return hash % HASH_TABLE_SIZE;
}

// Allocates a new hash map
struct HashMap *hashmap_new() {
    struct HashMap *map = malloc(sizeof(struct HashMap));
    memset(map->entries, 0, sizeof(map->entries));
    return map;
}

// Frees a hash map and all its entries
void hashmap_free(struct HashMap *map) {
    struct Entry *entry, *temp;
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        entry = map->entries[i];
        while (entry) {
            temp = entry;
            entry = entry->next;
            free(temp->key);
            free(temp);
        }
    }
    free(map);
}

// Adds a key-value pair to the hash map
void hashmap_put(struct HashMap *map, const char *key, char *value) {
    unsigned int index = hash(key);
    struct Entry *entry = map->entries[index];
    while (entry) {
        if (!strcmp(entry->key, key)) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    entry = malloc(sizeof(struct Entry));
    entry->key = strdup(key);
    entry->value = value;
    entry->next = map->entries[index];
    map->entries[index] = entry;
}

// Gets the value associated with a key, or returns NULL if not found
char* hashmap_get(struct HashMap *map, const char *key) {
    unsigned int index = hash(key);
    struct Entry *entry = map->entries[index];
    while (entry) {
        if (!strcmp(entry->key, key))
            return entry->value;
        entry = entry->next;
    }
    return NULL;
}
