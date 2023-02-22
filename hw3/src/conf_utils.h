#ifndef CONF_UTILS_H
#define CONF_UTILS_H

#include <stdbool.h>
#include "hmap_utils.h"

#define LINE_BUF_SIZE 1024
#define MAX_PEERS 50
#define KEY_BUF_SIZE 64
#define VALUE_BUF_SIZE 256
#define KEY_BUF_SIZE_STR "63"
#define VALUE_BUF_SIZE_STR "255"

typedef struct {
    char uuid[64];
    char last_seen[64];
    char name[256];
    char host[256];
    int frontend_port;
    int backend_port;
    int weight;
    bool is_online;
    char **file_paths;
    int num_file_paths;
} peer_info;

typedef struct {
    char uuid[64];
    char last_seen[64];
    char name[256];
    char host[256];
    int frontend_port;
    int backend_port;
    char content_dir[256];
    int weight;
    bool is_online;
    int num_file_paths;
    char **file_paths;
    int peer_count;
    peer_info peers[MAX_PEERS];
} node_config;

// Define a new struct to hold a list of node_config structs
// USED FOR HASHMAP: FILEPATH -> LIST OF NODE_CONFIGS THAT HOLD THE FILE
typedef struct node_config_list {
    node_config config;
    struct node_config_list *next;
} node_config_list;

typedef struct my_container {
    int data;
    struct my_container *next[10];
} MyContainer;

extern node_config global_config;
extern hash_table *network_map;

int read_config_file(const char *filename, node_config *config);

char* get_config_uuid(node_config *config);
char* get_json_config_uuid(node_config *config);

char* node_config_to_json(node_config *config, bool is_formatted);

char* get_config_value_by_key(node_config *config, const char *key);

char* get_config_value_by_key_json(node_config *config, const char *key, bool pretty);

void add_peer_from_string(node_config *config, const char *peer_string);

node_config* json_to_node_config(char* json_str);

hash_table* node_config_to_hashmap(node_config *config, int root_weight);
char* hashmap_to_json(hash_table *ht);

int find_peer_weight(node_config *config, char *uuid);

void update_peer_weights_last_seen(node_config *config, hash_table *ht, char *peer_uuid);

// Function to add a node_config struct to a node_config_list
node_config_list* add_node_config(node_config_list *list, node_config config);

void update_network_map(hash_table *network_map, node_config* config);

char* network_map_json(hash_table* network_map);

void hash_table_update_node_config(hash_table *ht, char *key, node_config *value);

char* get_config_fields_json(char** config_strings, int config_strings_count, char** peer_strings, int peer_strings_count, node_config *config);

char* get_all_configs_json(char** config_strings, int config_strings_count, char** peer_strings, int peer_strings_count, hash_table *ht);

#endif  // CONF_UTILS_H