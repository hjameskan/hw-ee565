#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "hashmap.h"

unsigned int hash(const char *str) {
    unsigned int result = 0;
    while (*str) {
        result = result * 31 + *str++;
    }
    return result % HASH_TABLE_SIZE;
}

struct HashMap *hashmap_new() {
    struct HashMap *map = (struct HashMap*)malloc(sizeof(struct HashMap));
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        map->entries[i] = NULL;
    }
    return map;
}

void hashmap_free(struct HashMap *map) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        struct Entry *entry = map->entries[i];
        while (entry) {
            struct Entry *next = entry->next;
            free(entry->key);
            free(entry->value);
            free(entry);
            entry = next;
        }
    }
    free(map);
}

void hashmap_put(struct HashMap *map, const char *key, char *value) {
    unsigned int index = hash(key);
    struct Entry *entry = (struct Entry*)malloc(sizeof(struct Entry));
    entry->key = strdup(key);
    entry->value = strdup(value);
    entry->next = map->entries[index];
    map->entries[index] = entry;
}

char* hashmap_get(struct HashMap *map, const char *key) {
    unsigned int index = hash(key);
    struct Entry *entry = map->entries[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    return NULL;
}

void hashmap_print_all(struct HashMap *map) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        struct Entry *entry = map->entries[i];
        while (entry) {
            printf("%s: %s\n", entry->key, entry->value);
            fflush(stdout);
            entry = entry->next;
        }
    }
}
