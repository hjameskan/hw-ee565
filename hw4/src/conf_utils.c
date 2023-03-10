#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <stdbool.h>
// #include <uuid/uuid.h>
#include <uuid4.h>
#include <time.h>
#include <unistd.h>

#include "conf_utils.h"
#include "hmap_utils.h"

#define MAX_LINE_LENGTH 1024
#define MAX_KEY_LENGTH 256
#define MAX_VALUE_LENGTH 256
#define MAX_HASHTABLE_SIZE 100

// Function to parse a config file and return a node_config struct
int read_config_file(const char *filename, node_config *config) {
    FILE *fp;
    char line[LINE_BUF_SIZE];
    char key[KEY_BUF_SIZE], value[VALUE_BUF_SIZE];
    int line_num = 0, peer_num;
    cJSON *root, *peers, *peer;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening config file");
        return 1;
    }

    root = cJSON_CreateObject();

    while (fgets(line, LINE_BUF_SIZE, fp) != NULL) {
        line_num++;
        if (line[0] == '#' || line[0] == '\n') {
            continue;  // Skip comments and blank lines
        }
        if (sscanf(line, "%[^= ] = %[^\n]", key, value) != 2) {
            fprintf(stderr, "Error on line %d: Invalid format\n", line_num);
            fclose(fp);
            return 1;
        }
        if (strcmp(key, "uuid") == 0) {
            strncpy(config->uuid, value, sizeof(config->uuid));
        } else if (strcmp(key, "name") == 0) {
            strncpy(config->name, value, sizeof(config->name));
        } else if (strcmp(key, "frontend_port") == 0) {
            config->frontend_port = atoi(value);
        } else if (strcmp(key, "backend_port") == 0) {
            config->backend_port = atoi(value);
        } else if (strcmp(key, "content_dir") == 0) {
            strncpy(config->content_dir, value, sizeof(config->content_dir));
        } else if (strcmp(key, "peer_count") == 0) {
            config->peer_count = atoi(value);
            peers = cJSON_CreateArray();
            cJSON_AddItemToObject(root, "peers", peers);
        } else if (strncmp(key, "peer_", 5) == 0) {
            peer_num = atoi(key + 5);
            if (peer_num >= config->peer_count) {
                fprintf(stderr, "Error on line %d: Invalid peer number\n", line_num);
                fclose(fp);
                return 1;
            }
            peer = cJSON_CreateObject();
            cJSON_AddItemToArray(peers, peer);
            if (sscanf(value, "%[^,],%[^,],%d,%d,%d", config->peers[peer_num].uuid,
                       config->peers[peer_num].host, &config->peers[peer_num].frontend_port,
                       &config->peers[peer_num].backend_port, &config->peers[peer_num].weight) != 5) {
                fprintf(stderr, "Error on line %d: Invalid peer format\n", line_num);
                fclose(fp);
                return 1;
            } else {
                sprintf(config->peers[peer_num].name, "Peer %d", peer_num);
            }
            cJSON_AddStringToObject(peer, "uuid", config->peers[peer_num].uuid);
            cJSON_AddStringToObject(peer, "name", config->peers[peer_num].name);
            cJSON_AddStringToObject(peer, "host", config->peers[peer_num].host);
            cJSON_AddNumberToObject(peer, "frontend_port", config->peers[peer_num].frontend_port);
            cJSON_AddNumberToObject(peer, "backend_port", config->peers[peer_num].backend_port);
            cJSON_AddNumberToObject(peer, "weight", config->peers[peer_num].weight);
        } else {
            fprintf(stderr, "Error on line %d: Invalid key '%s'\n", line_num, key);
            cJSON_Delete(root); // Free memory used by cJSON
            fclose(fp);
            return 0;
        }
    }
}




char* get_config_uuid(node_config *config) {
    return config->uuid;
}

char* get_json_config_uuid(node_config *config) {
    cJSON *root, *peers_array, *peer_obj;
    char *json_str;

    FILE* file = fopen("node.conf", "r");
    if (file != NULL) { // file exists, do original function actions
        fclose(file);

        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "uuid", config->uuid);
        json_str = cJSON_Print(root);
        cJSON_Delete(root);

        return json_str;
    } else { // file not exist, generate node.conf file and write config
        fclose(file);
        UUID4_STATE_T state;
        UUID4_T uuid4t;
        uuid4_seed(&state);
        uuid4_gen(&state, &uuid4t);
        char buffer[UUID4_STR_BUFFER_SIZE];
        if (!uuid4_to_s(uuid4t, buffer, sizeof(buffer)))
            exit(EXIT_FAILURE);


        file = fopen("node.conf", "w");
        char *uuid = buffer;
        char *name = "node1";
        int frontend_port = 8345;
        int backend_port = 8346;
        char *content_dir = "content/";
        int peer_count = 1;
        // int peer_count = 2;
        char *peer_0 = "24f22a83-16f4-4bd5-af63-b5c6e979dbb,localhost,8345,8346,10";
        // char *peer_1 = "3d2f4e34-6d21-4dda-aa78-796e3507903c,localhost,8345, 8346,20";

        fprintf(file, "uuid = %s\n", uuid);
        fprintf(file, "name = %s\n", name);
        fprintf(file, "frontend_port = %d\n", frontend_port);
        fprintf(file, "backend_port = %d\n", backend_port);
        fprintf(file, "content_dir = %s\n", content_dir);
        fprintf(file, "peer_count = %d\n", peer_count);
        fprintf(file, "peer_0 = %s\n", peer_0);
        // fprintf(file, "peer_1 = %s\n", peer_1);
        fclose(file);

        root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "uuid", uuid);
        json_str = cJSON_Print(root);
        cJSON_Delete(root);


        return json_str;
    }
}

char* node_config_to_json(node_config *config, bool is_formatted, hash_table *config_files) {
    is_formatted == NULL ? false : true;

    cJSON *root, *peers_array, *peer_obj;
    char *json_str;

    // Create the root object
    root = cJSON_CreateObject();

    // Add the UUID
    cJSON_AddStringToObject(root, "uuid", config->uuid);
    cJSON_AddStringToObject(root, "last_seen", config->last_seen);

    // Add the name
    cJSON_AddStringToObject(root, "name", config->name);

    // Add the frontend port
    cJSON_AddNumberToObject(root, "frontend_port", config->frontend_port);

    // Add the backend port
    cJSON_AddNumberToObject(root, "backend_port", config->backend_port);

    // Add the content directory
    cJSON_AddStringToObject(root, "content_dir", config->content_dir);

    // Add the peer count
    cJSON_AddNumberToObject(root, "peer_count", config->peer_count);

    // Create an array for the peers
    peers_array = cJSON_CreateArray();

    // Add each peer object to the peers array
    for (int i = 0; i < config->peer_count; i++) {
        // Create a new object for the peer
        peer_obj = cJSON_CreateObject();

        // Add the UUID
        cJSON_AddStringToObject(peer_obj, "uuid", config->peers[i].uuid);
        cJSON_AddStringToObject(peer_obj, "last_seen", config->peers[i].last_seen);

        // Add the hostname
        cJSON_AddStringToObject(peer_obj, "name", config->peers[i].name);
        cJSON_AddStringToObject(peer_obj, "host", config->peers[i].host);

        // Add the frontend port
        cJSON_AddNumberToObject(peer_obj, "frontend_port", config->peers[i].frontend_port);

        // Add the backend port
        cJSON_AddNumberToObject(peer_obj, "backend_port", config->peers[i].backend_port);

        // Add the weight
        cJSON_AddNumberToObject(peer_obj, "weight", config->peers[i].weight);

        // Add the peer object to the peers array
        cJSON_AddItemToArray(peers_array, peer_obj);
    }

    // Add the peers array to the root object
    cJSON_AddItemToObject(root, "peers", peers_array);

    // Add config files
    if(config_files != NULL /*&& config_files->size > 0*/) {
        printf("config_files: %d\n", config_files->size);
        cJSON *config_files_ht = file_map_to_json(config_files);
        cJSON_AddItemToObject(root, "file_map", config_files_ht);
    }

    // Generate the JSON string
    json_str = is_formatted ? cJSON_Print(root) : cJSON_PrintUnformatted(root);

    // Free the cJSON objects
    cJSON_Delete(root);


    return json_str;
}

// Convert a file_info struct to a cJSON object
cJSON* file_info_to_json(file_info *info) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "path", info->path);
    // cJSON_AddStringToObject(json, "hash", info->hash);
    // cJSON_AddNumberToObject(json, "size", info->size);
    // cJSON_AddNumberToObject(json, "num_peers", info->num_peers);
    // cJSON *peer_uuids = cJSON_CreateArray();
    // for (int i = 0; i < info->num_peers; i++) {
    //     cJSON_AddItemToArray(peer_uuids, cJSON_CreateString(info->peer_uuids[i]));
    // }
    // cJSON_AddItemToObject(json, "peer_uuids", peer_uuids);
    char *s = cJSON_Print(json);
    return json;
}

// Convert the file_map hash table to a cJSON object
cJSON* file_map_to_json(hash_table *file_map) {
    cJSON *json = cJSON_CreateObject();
    for (int i = 0; i < file_map->size; i++) {
        hash_node *node = file_map->buckets[i];
        while (node) {
            char *path = (char*)node->key;
            file_info *info = (file_info*)node->value;
            cJSON_AddItemToObject(json, path, file_info_to_json(info));
            node = node->next;
        }
    }
    return json;
}

char* get_config_value_by_key(node_config *config, const char *key) {
    if (strcmp(key, "uuid") == 0) {
        return strdup(config->uuid);
    } else if (strcmp(key, "name") == 0) {
        return strdup(config->name);
    } else if (strcmp(key, "frontend_port") == 0) {
        char *value_str = (char *) malloc(sizeof(char) * VALUE_BUF_SIZE);
        snprintf(value_str, VALUE_BUF_SIZE, "%d", config->frontend_port);
        return value_str;
    } else if (strcmp(key, "backend_port") == 0) {
        char *value_str = (char *) malloc(sizeof(char) * VALUE_BUF_SIZE);
        snprintf(value_str, VALUE_BUF_SIZE, "%d", config->backend_port);
        return value_str;
    } else if (strcmp(key, "content_dir") == 0) {
        return strdup(config->content_dir);
    } else if (strcmp(key, "peer_count") == 0) {
        char *value_str = (char *) malloc(sizeof(char) * VALUE_BUF_SIZE);
        snprintf(value_str, VALUE_BUF_SIZE, "%d", config->peer_count);
        return value_str;
    } else if (strncmp(key, "peers[", 6) == 0) {
        int index = atoi(key + 6);
        if (index < 0 || index >= config->peer_count) {
            return NULL;
        }
        if (strcmp(key + strlen(key) - 7, "uuid") == 0) {
            return strdup(config->peers[index].uuid);
        } else if (strcmp(key + strlen(key) - 9, "name") == 0) {
            return strdup(config->peers[index].name);
        } else if (strcmp(key + strlen(key) - 9, "host") == 0) {
            return strdup(config->peers[index].host);
        } else if (strcmp(key + strlen(key) - 15, "frontend_port") == 0) {
            char *value_str = (char *) malloc(sizeof(char) * VALUE_BUF_SIZE);
            snprintf(value_str, VALUE_BUF_SIZE, "%d", config->peers[index].frontend_port);
            return value_str;
        } else if (strcmp(key + strlen(key) - 13, "backend_port") == 0) {
            char *value_str = (char *) malloc(sizeof(char) * VALUE_BUF_SIZE);
            snprintf(value_str, VALUE_BUF_SIZE, "%d", config->peers[index].backend_port);
            return value_str;
        } else if (strcmp(key + strlen(key) - 6, "weight") == 0) {
            char *value_str = (char *) malloc(sizeof(char) * VALUE_BUF_SIZE);
            snprintf(value_str, VALUE_BUF_SIZE, "%d", config->peers[index].weight);
            return value_str;
        } else {
            return NULL;
        }
    } else {
        return NULL;
    }
}

char* get_config_value_by_key_json(node_config *config, const char *key, bool pretty) {
    pretty = pretty == NULL ? false : pretty;

    cJSON *root = cJSON_CreateObject();
    cJSON *value;

    // Handle peers separately
    if (strcmp(key, "peers") == 0) {
        cJSON *peers_array = cJSON_CreateArray();
        for (int i = 0; i < config->peer_count; i++) {
            cJSON *peer_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(peer_obj, "uuid", config->peers[i].uuid);
            cJSON_AddStringToObject(peer_obj, "last_seen", config->peers[i].last_seen);
            cJSON_AddStringToObject(peer_obj, "name", config->peers[i].name);
            cJSON_AddStringToObject(peer_obj, "host", config->peers[i].host);
            cJSON_AddNumberToObject(peer_obj, "frontend_port", config->peers[i].frontend_port);
            cJSON_AddNumberToObject(peer_obj, "backend_port", config->peers[i].backend_port);
            cJSON_AddNumberToObject(peer_obj, "weight", config->peers[i].weight);
            cJSON_AddItemToArray(peers_array, peer_obj);
        }
        value = peers_array;
    } else {
        // Handle other keys
        if (strcmp(key, "uuid") == 0) {
            value = cJSON_CreateString(config->uuid);
        } else if (strcmp(key, "last_seen") == 0) {
            value = cJSON_CreateString(config->last_seen);
        } else if (strcmp(key, "name") == 0) {
            value = cJSON_CreateString(config->name);
        }  else if (strcmp(key, "frontend_port") == 0) {
            value = cJSON_CreateNumber(config->frontend_port);
        } else if (strcmp(key, "backend_port") == 0) {
            value = cJSON_CreateNumber(config->backend_port);
        } else if (strcmp(key, "content_dir") == 0) {
            value = cJSON_CreateString(config->content_dir);
        } else if (strcmp(key, "peer_count") == 0) {
            value = cJSON_CreateNumber(config->peer_count);
        } else {
            printf("Error: Invalid key: %s\n", key);
            return NULL;
        }
    }

    // Add key-value pair to the JSON object
    cJSON_AddItemToObject(root, key, value);

    return pretty ? cJSON_Print(root) : cJSON_PrintUnformatted(root);
}

int add_peer_from_string(node_config *config, const char *peer_string) {
    // Parse the peer data from the input string
    char uuid[37];
    char host[256];
    int frontend_port, backend_port, weight;
    printf("peer_string: %s \n", peer_string);
    int parsed = sscanf(peer_string, "/peer/addneighbor?uuid=%36[^&]&host=%255[^&]&frontend=%d&backend=%d&metric=%d",
                        uuid, host, &frontend_port, &backend_port, &weight);
    if (parsed != 5) {
        // Failed to parse the input string
        fprintf(stderr, "Error: Invalid peer string format\n");
        return -1;
    }
    
    // Add the new peer to the config
    if (config->peer_count >= MAX_PEERS) {
        // Reached the maximum number of peers
        fprintf(stderr, "Error: Maximum number of peers reached\n");
        return -1;
    }
    for (int i = 0; i < config->peer_count; i++) {
        if (strcmp(config->peers[i].uuid, uuid) == 0) {
            fprintf(stderr, "Error: UUID for neighbor already exists\n");
            return -2;
        }
    }

    // This part is for updating the *config peers 
    config->peers[config->peer_count] = (peer_info) {
        "none",
        "",
        "",
        0,
        0,
        0
    };
    // printf("config->peers[config->peer_count]: %s \n", config->peers[config->peer_count].uuid);
    peer_info *peer = &config->peers[config->peer_count];
    // printf("peer->uuid: %s \n", peer->uuid);
    strncpy(peer->uuid, uuid, sizeof(peer->uuid));
    strncpy(peer->name, "", sizeof(peer->name));  // Not provided in input string
    strncpy(peer->host, host, sizeof(peer->host));
    peer->frontend_port = frontend_port;
    peer->backend_port = backend_port;
    peer->weight = weight;
    // printf("peer->uuid: %s \n", peer->uuid);
    
    config->peer_count++;

    return 0;
}

node_config* url_path_to_config(char *url_path) {
    // Parse the peer data from the input string
    char uuid[37];
    char host[256];
    int frontend_port, backend_port, weight;
    int parsed = sscanf(url_path, "/peer/addneighbor?uuid=%36[^&]&host=%255[^&]&frontend=%d&backend=%d&metric=%d",
                        uuid, host, &frontend_port, &backend_port, &weight);
    if (parsed != 5) {
        // Failed to parse the input string
        fprintf(stderr, "Error: Invalid peer string format\n");
        return -1;
    }

    // This part is for the network map
    // Create a new JSON object
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        printf("Failed to create JSON object\n");
        return 1;
    }

    // Add each field to the JSON object
    cJSON_AddStringToObject(json, "uuid", uuid);
    cJSON_AddStringToObject(json, "last_seen", "");
    cJSON_AddStringToObject(json, "name", "");
    cJSON_AddStringToObject(json, "host", host);
    cJSON_AddNumberToObject(json, "frontend_port", frontend_port);
    cJSON_AddNumberToObject(json, "backend_port", backend_port);
    cJSON_AddNumberToObject(json, "weight", weight);

    cJSON *file_paths_array = cJSON_CreateArray();
    if (file_paths_array == NULL) {
        printf("Failed to create JSON array\n");
        return 1;
    }

    // Add each file path to the file_paths_array
    // for (int i = 0; i < peer->num_file_paths; i++) {
    //     cJSON_AddItemToArray(file_paths_array, cJSON_CreateString(peer->file_paths[i]));
    // }

    cJSON_AddItemToObject(json, "file_paths", file_paths_array);

    // Print the JSON object
    char *json_str = cJSON_PrintUnformatted(json);
    node_config *peer_config = json_to_node_config(json_str);
    if(peer_config == NULL) {
        printf("peer_config is NULL \n");
        fflush(stdout);
        return -1;
    }

    // Free memory
    cJSON_Delete(json);
    free(json_str);

    return peer_config;

}

node_config* json_to_node_config(char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        printf("Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }

    node_config* config = (node_config*)malloc(sizeof(node_config));
    if (!config) {
        printf("Failed to allocate memory for node_config\n");
        cJSON_Delete(root);
        return NULL;
    }

    // Extract fields from JSON
    cJSON* uuid = cJSON_GetObjectItem(root, "uuid");
    // cJSON* last_seen = cJSON_GetObjectItem(root, "last_seen");
    cJSON* name = cJSON_GetObjectItem(root, "name");
    cJSON* frontend_port = cJSON_GetObjectItem(root, "frontend_port");
    cJSON* backend_port = cJSON_GetObjectItem(root, "backend_port");
    cJSON* content_dir = cJSON_GetObjectItem(root, "content_dir");
    cJSON* peer_count = cJSON_GetObjectItem(root, "peer_count");
    cJSON* peers = cJSON_GetObjectItem(root, "peers");

    // Convert fields to node_config struct
    if (uuid && uuid->valuestring) {
        strcpy(config->uuid, uuid->valuestring);
    }
    // if (last_seen && last_seen->valuestring) {
    //     strcpy(config->last_seen, last_seen->valuestring);
    // }
    if (name && name->valuestring) {
        strcpy(config->name, name->valuestring);
    }
    if (frontend_port) {
        config->frontend_port = frontend_port->valueint;
    }
    if (backend_port) {
        config->backend_port = backend_port->valueint;
    }
    if (content_dir && content_dir->valuestring) {
        strcpy(config->content_dir, content_dir->valuestring);
    }
    if (peer_count) {
        config->peer_count = peer_count->valueint;
    }
    if (peers) {
        int num_peers = cJSON_GetArraySize(peers);
        if (num_peers > MAX_PEERS) {
            printf("Too many peers, maximum is %d\n", MAX_PEERS);
            free(config);
            cJSON_Delete(root);
            return NULL;
        }
        for (int i = 0; i < num_peers; i++) {
            cJSON* peer = cJSON_GetArrayItem(peers, i);
            if (peer) {
                cJSON* peer_uuid = cJSON_GetObjectItem(peer, "uuid");
                // cJSON* peer_last_seen = cJSON_GetObjectItem(peer, "last_seen");
                cJSON* peer_name = cJSON_GetObjectItem(peer, "name");
                cJSON* peer_host = cJSON_GetObjectItem(peer, "host");
                cJSON* peer_frontend_port = cJSON_GetObjectItem(peer, "frontend_port");
                cJSON* peer_backend_port = cJSON_GetObjectItem(peer, "backend_port");
                cJSON* peer_weight = cJSON_GetObjectItem(peer, "weight");
                if (peer_uuid && peer_uuid->valuestring) {
                    strcpy(config->peers[i].uuid, peer_uuid->valuestring);
                }
                // if (true) { // just do: add last_seen time
                //     strcpy(config->peers[i].last_seen, last_seen_str);
                // }
                if (peer_name && peer_name->valuestring) {
                    strcpy(config->peers[i].name, peer_name->valuestring);
                }
                if (peer_host && peer_host->valuestring) {
                    strcpy(config->peers[i].host, peer_host->valuestring);
                }
                if (peer_frontend_port) {
                    config->peers[i].frontend_port = peer_frontend_port->valueint;
                }
                if (peer_backend_port) {
                    config->peers[i].backend_port = peer_backend_port->valueint;
                }
                if (peer_weight) {
                    config->peers[i].weight = peer_weight->valueint;
                }
            }
        }
    }

    cJSON_Delete(root);
    return config;
}

hash_table* json_to_files_map(char* json_str) {
    hash_table* ht_files_map = hash_table_create(MAX_HASHTABLE_SIZE);

    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        printf("Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        return NULL;
    }

    cJSON* file_map = cJSON_GetObjectItem(root, "file_map");
    if (!file_map) {
        printf("Error: no 'file_map' field in JSON\n");
        cJSON_Delete(root);
        return NULL;
    }

    cJSON* file_item;
    cJSON_ArrayForEach(file_item, file_map) {
        const char* file_path = file_item->string;
        const char* file_real_path = cJSON_GetObjectItem(file_item, "path")->valuestring;
        const file_info f_info = {0};
        strcpy(f_info.path, file_real_path);
        hash_table_put_str(ht_files_map, file_path, (void *)&f_info, strlen(file_real_path));
    }

    cJSON_Delete(root);
    // printf("SLEEPING FOR 10000 SECONDS");
    // fflush(stdout);
    // sleep(10000);

    return ht_files_map;
}

void update_peer_to_files_map(hash_table *local_files_map, hash_table *peers_ht_files_map, node_config *peer_config) {
    printf("update_peer_to_files_map\n");
    fflush(stdout);
    for(int i = 0; i < peers_ht_files_map->size; i++){
        if(peers_ht_files_map->buckets[i] != NULL){
            hash_node *node = peers_ht_files_map->buckets[i];
            while(node != NULL){
                file_info *peers_file_info = (file_info *)node->value;
                char *file_path = node->key;
                file_info *file_path_file_info = hash_table_get(local_files_map, file_path, strlen(file_path));
                if (file_path_file_info != NULL && file_path_file_info->peers != NULL) {
                    bool isFound = hash_table_get(file_path_file_info->peers, peer_config->uuid, strlen(peer_config->uuid));
                    if (isFound == NULL) {
                        hash_table_put(file_path_file_info->peers, peer_config->uuid, true, strlen(peer_config->uuid));
                    }
                } 
                else {
                    printf("HERE------------------------\n"); fflush(stdout);
                    printf("this is file_path: %s \n", file_path);
                    printf("HERE------------------------\n"); fflush(stdout);
                    file_info *f = malloc(sizeof(file_info));
                    f->peers = hash_table_create(MAX_HASHTABLE_SIZE);
                    strcpy(f->path, peers_file_info->path);
                    bool isFound = hash_table_get(f->peers, peer_config->uuid, strlen(peer_config->uuid));
                    if (!isFound) {
                        hash_table_put(f->peers, peer_config->uuid, true, strlen(peer_config->uuid));
                    }
                    fflush(stdout);
                    // sprintf(f->path, "%s", file_path);
                    hash_table_put_str(local_files_map, file_path, f, strlen(file_path));
                }
                node = node->next;
            }
        }
    }

    return;
}

hash_table* node_config_to_hashmap(node_config *config, int root_weight) {
    hash_table *ht = hash_table_create(config->peer_count + 1);
    if (ht == NULL) {
        return NULL;
    }

    // Add the initial peer to the hash table
    peer_info *initial_peer = malloc(sizeof(peer_info));
    if (initial_peer == NULL) {
        hash_table_destroy(ht);
        return NULL;
    }
    memcpy(initial_peer, config, sizeof(peer_info));
    initial_peer->weight = root_weight == NULL ? 0 : root_weight;

    hash_table_put(ht, initial_peer->uuid, initial_peer, strlen(initial_peer->uuid));

    // Add each peer to the hash table
    for (int i = 0; i < config->peer_count; i++) {
        peer_info peer = config->peers[i];

        // Check if the peer is already in the hash table
        peer_info *existing_peer = hash_table_get(ht, peer.uuid, strlen(peer.uuid));

        // If the peer is not in the hash table, create a new peer_info object for it
        if (existing_peer == NULL) {
            peer_info *new_peer = malloc(sizeof(peer_info));
            if (new_peer == NULL) {
                hash_table_destroy(ht);
                return NULL;
            }
            memcpy(new_peer, &peer, sizeof(peer_info));
            hash_table_put(ht, new_peer->uuid, new_peer, strlen(new_peer->uuid));
        } else {
            // If the peer is already in the hash table, update its weight if the provided weight is higher
            if (strcmp(existing_peer->uuid, config->uuid) != 0 && peer.weight < existing_peer->weight) {
                existing_peer->weight = peer.weight;
            }
        }
    }

    return ht;
}



// Prints the hash table as a JSON string
char* hashmap_to_json(hash_table *ht) {
    cJSON *root = cJSON_CreateObject();
    for (int i = 0; i < ht->size; i++) {
        hash_node *node = ht->buckets[i];
        while (node != NULL) {
            peer_info *config = (peer_info*) node->value;
            cJSON *item = cJSON_CreateObject();
            cJSON_AddStringToObject(item, "uuid", config->uuid);
            cJSON_AddStringToObject(item, "name", config->name);
            cJSON_AddStringToObject(item, "host", config->host);
            cJSON_AddNumberToObject(item, "frontend_port", config->frontend_port);
            cJSON_AddNumberToObject(item, "backend_port", config->backend_port);
            cJSON_AddNumberToObject(item, "weight", config->weight);
            // cJSON_AddNumberToObject(item, "peer_count", config->peer_count);
            // cJSON_AddStringToObject(item, "content_dir", config->content_dir);
            // cJSON *peers = cJSON_CreateArray();
            // for (int j = 0; j < config->peer_count; j++) {
            //     cJSON *peer = cJSON_CreateObject();
            //     cJSON_AddStringToObject(peer, "uuid", config->peers[j].uuid);
            //     cJSON_AddStringToObject(peer, "name", config->peers[j].name);
            //     cJSON_AddStringToObject(peer, "host", config->peers[j].host);
            //     cJSON_AddNumberToObject(peer, "frontend_port", config->peers[j].frontend_port);
            //     cJSON_AddNumberToObject(peer, "backend_port", config->peers[j].backend_port);
            //     cJSON_AddNumberToObject(peer, "weight", config->peers[j].weight);
            //     cJSON_AddItemToArray(peers, peer);
            // }
            // cJSON_AddItemToObject(item, "peers", peers);
            cJSON_AddItemToObject(root, config->uuid, item);
            node = node->next;
        }
    }
    char *json_str = cJSON_Print(root);
    // char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

int find_peer_weight(node_config *config, char *uuid) {
    for (int i = 0; i < config->peer_count; i++) {
        if (strcmp(config->peers[i].uuid, uuid) == 0) {
            return config->peers[i].weight;
        }
    }
    return -1; // Return -1 if the peer with the given UUID is not found
}

void update_peer_weights_last_seen(node_config *config, hash_table *ht, node_config *peer_config) {
    // since we have the peer record, update the last_seen field
    // Get current time as time_t
    time_t current_time = time(NULL);
    // Convert time_t to string
    char last_seen_str[20];
    strftime(last_seen_str, sizeof(last_seen_str), "%Y-%m-%d %H:%M:%S", localtime(&current_time));
    // Find the weight of the peer with the given uuid in the config
    char *peer_uuid = peer_config->uuid;
    int given_weight = 0;
    for (int i = 0; i < config->peer_count; i++) {
        if (strcmp(config->peers[i].uuid, peer_uuid) == 0) {
            given_weight = config->peers[i].weight;
            strcpy(config->peers[i].last_seen, last_seen_str);
            break;
        }
    }
    char lookup_uuid[128];
    strcpy(lookup_uuid, config->uuid);
    peer_info *peers_config_of_me;
    if(given_weight == 0 && (peers_config_of_me= hash_table_get(ht, lookup_uuid, strlen(lookup_uuid))) != NULL) {
        given_weight = peers_config_of_me->weight;
    }

    // Update the weights of the peers in the config based on the hash table
    for (int i = 0; i < ht->size; i++) {
        hash_node *node = ht->buckets[i];
        while (node != NULL) {
            peer_info *peer = (peer_info *) node->value;
            if(strcmp(peer->uuid, config->uuid) == 0) {
                node = node->next;
                continue;
            }

            // Check if the peer is already in the config
            int index = -1;
            for (int j = 0; j < config->peer_count; j++) {
                if (strcmp(config->peers[j].uuid, peer->uuid) == 0) {
                    index = j;
                    break;
                }
            }

            // If the peer is not in the config, add it
            if (index == -1) {
                if (config->peer_count >= MAX_PEERS) {
                    break;
                }
                peer_info *new_peer = &config->peers[config->peer_count++];
                strcpy(new_peer->uuid, peer->uuid);
                // strcpy(new_peer->last_seen, peer->last_seen);
                strcpy(new_peer->name, peer->name);
                strcpy(new_peer->host, peer->host);
                new_peer->frontend_port = peer->frontend_port;
                new_peer->backend_port = peer->backend_port;
                // strcpy(new_peer->content_dir, peer->content_dir);
                new_peer->weight = given_weight + peer->weight;
            } else if (strcmp(config->peers[index].uuid, peer_uuid) != 0) {
                // If the peer is already in the config, update its weight if the new weight is lower
                int new_weight = given_weight + peer->weight;
                if (new_weight < config->peers[index].weight) {
                    config->peers[index].weight = new_weight;
                }
                // strcpy(config->peers[index].last_seen, peer->last_seen);
            }

            node = node->next;
        }
    }
}

// Function to add a node_config struct to a node_config_list
node_config_list* add_node_config(node_config_list *list, node_config config) {
    node_config_list *new_node = (node_config_list*) malloc(sizeof(node_config_list));
    new_node->config = config;
    new_node->next = list;
    return new_node;
}

void update_network_map(hash_table *ht, node_config* config) {
    // Check if the node's uuid is in the network map
    void* old_config = hash_table_get(ht, config->uuid, strlen(config->uuid));

    if (old_config != NULL) {
        // If it exists, remove the old entry and replace it with the new config
        hash_table_delete(ht, config->uuid, strlen(config->uuid));
        fflush(stdout);
    }

    // Add the new config to the network map
    hash_table_put(ht, config->uuid, config, strlen(config->uuid));
}

void hash_table_update_node_config(hash_table *ht, char *key, node_config *value, int size) {
    if(size == NULL){
        size = sizeof(key);
    }
    // Remove old node_config entry
    hash_table_delete(ht, key, size);
    // Add new node_config entry
    hash_table_put(ht, key, value, size);
}


// Print the network map hash table as a formatted JSON string
char* network_map_json(hash_table* network_map) {
    // Create a JSON object to hold the network map
    cJSON* network_map_json = cJSON_CreateObject();

    // Iterate through the network map hash table
    for (int i = 0; i < network_map->size; i++) {
        hash_node* node = network_map->buckets[i];
        while (node != NULL) {
            node_config* config = (node_config*)node->value;

            // Create a JSON object for the node config
            cJSON* config_json = cJSON_CreateObject();

            // Add the config fields to the JSON object
            cJSON_AddStringToObject(config_json, "uuid", config->uuid);
            cJSON_AddStringToObject(config_json, "last_seen", config->last_seen);
            cJSON_AddStringToObject(config_json, "name", config->name);
            cJSON_AddStringToObject(config_json, "host", config->host);
            cJSON_AddNumberToObject(config_json, "frontend_port", config->frontend_port);
            cJSON_AddNumberToObject(config_json, "backend_port", config->backend_port);
            cJSON_AddStringToObject(config_json, "content_dir", config->content_dir);
            cJSON_AddNumberToObject(config_json, "weight", config->weight);
            cJSON_AddBoolToObject(config_json, "is_online", config->is_online);

            // Add the file paths to the JSON object as an array
            cJSON* file_paths_json = cJSON_CreateStringArray((const char**)config->file_paths, config->num_file_paths);
            cJSON_AddItemToObject(config_json, "file_paths", file_paths_json);

            // Add the peer configs to the JSON object as an array
            cJSON_AddNumberToObject(config_json, "peer_count", config->peer_count);
            cJSON* peers_json = cJSON_CreateArray();
            for (int j = 0; j < config->peer_count; j++) {
                peer_info* peer = &config->peers[j];
                cJSON* peer_json = cJSON_CreateObject();
                cJSON_AddStringToObject(peer_json, "uuid", peer->uuid);
                cJSON_AddStringToObject(peer_json, "last_seen", peer->last_seen);
                cJSON_AddStringToObject(peer_json, "name", peer->name);
                cJSON_AddStringToObject(peer_json, "host", peer->host);
                cJSON_AddNumberToObject(peer_json, "frontend_port", peer->frontend_port);
                cJSON_AddNumberToObject(peer_json, "backend_port", peer->backend_port);
                cJSON_AddNumberToObject(peer_json, "weight", peer->weight);
                cJSON_AddBoolToObject(peer_json, "is_online", peer->is_online);
                cJSON_AddItemToArray(peers_json, peer_json);
            }
            cJSON_AddItemToObject(config_json, "peers", peers_json);

            // Add the config JSON object to the network map JSON object
            cJSON_AddItemToObject(network_map_json, config->uuid, config_json);

            node = node->next;
        }
    }

    // Convert the JSON object to a formatted string and print it
    char* network_map_str = cJSON_Print(network_map_json);
    // printf("%s\n", network_map_str);
    // free(network_map_str);
    // cJSON_Delete(network_map_json);
    return network_map_str;
}

char* get_config_fields_json(char** config_strings, int config_strings_count, char** peer_strings, int peer_strings_count, node_config *config) {
    printf("config_strings_count: %d \t peer_strings_count: %d \n", config_strings_count, peer_strings_count);
    bool has_peer_field = false;
    cJSON *json_root = cJSON_CreateObject();
    // Add node_config fields
    for (int i = 0; i < config_strings_count; i++) {
        printf("%d", i);
        printf("\n");
        fflush(stdout);
        if (strcmp(config_strings[i], "uuid") == 0) {
            cJSON_AddStringToObject(json_root, "uuid", config->uuid);
        }
        if (strcmp(config_strings[i], "name") == 0) {
            cJSON_AddStringToObject(json_root, "name", config->name);
        }
        if (strcmp(config_strings[i], "host") == 0) {
            cJSON_AddStringToObject(json_root, "host", config->host);
        }
        if (strcmp(config_strings[i], "content_dir") == 0) {
            cJSON_AddStringToObject(json_root, "content_dir", config->content_dir);
        }
        if (strcmp(config_strings[i], "frontend_port") == 0) {
            cJSON_AddNumberToObject(json_root, "frontend_port", config->frontend_port);
        }
        if (strcmp(config_strings[i], "backend_port") == 0) {
            cJSON_AddNumberToObject(json_root, "backend_port", config->backend_port);
        }
        if (strcmp(config_strings[i], "weight") == 0) {
            cJSON_AddNumberToObject(json_root, "weight", config->weight);
        }
        if (strcmp(config_strings[i], "peer_count") == 0) {
            cJSON_AddNumberToObject(json_root, "peer_count", config->weight);
        }
        if (strcmp(config_strings[i], "peers") == 0) {
            has_peer_field = true;
        }
        // if (strcmp(config_strings[i], "num_file_paths") == 0) {
        //     cJSON_AddNumberToObject(json_root, "num_file_paths", config->num_file_paths);
        // }
        // for (int j = 0; j < config->num_file_paths; j++) {
        //     if (strcmp(config_strings[i], config->file_paths[j]) == 0) {
        //         char field_name[20];
        //         sprintf(field_name, "file_path_%d", j);
        //         cJSON_AddStringToObject(json_root, field_name, config->file_paths[j]);
        //     }
        // }
    }

    // Add peer_info array
    cJSON *peers_array = cJSON_CreateArray();
    for (int i = 0; i < config->peer_count; i++) {
        cJSON *peer = cJSON_CreateObject();
        peer_info *peer_info = &config->peers[i];

        for (int j = 0; j < peer_strings_count; j++) {
            if (strcmp(peer_strings[j], "uuid") == 0) {
                cJSON_AddStringToObject(peer, "uuid", peer_info->uuid);
            }
            if (strcmp(peer_strings[j], "name") == 0) {
                cJSON_AddStringToObject(peer, "name", peer_info->name);
            }
            if (strcmp(peer_strings[j], "host") == 0) {
                cJSON_AddStringToObject(peer, "host", peer_info->host);
            }
            if (strcmp(peer_strings[j], "frontend_port") == 0) {
                cJSON_AddNumberToObject(peer, "frontend_port", peer_info->frontend_port);
            }
            if (strcmp(peer_strings[j], "backend_port") == 0) {
                cJSON_AddNumberToObject(peer, "backend_port", peer_info->backend_port);
            }
            if (strcmp(peer_strings[j], "weight") == 0) {
                cJSON_AddNumberToObject(peer, "weight", peer_info->weight);
            }
            if (strcmp(peer_strings[j], "is_online") == 0) {
                cJSON_AddBoolToObject(peer, "is_online", peer_info->is_online);
            }
            // if (strcmp(peer_strings[j], "num_file_paths") == 0) {
            //     cJSON_AddNumberToObject(peer, "num_file_paths", peer_info->num_file_paths);
            // }
            // for (int k = 0; k < peer_info->num_file_paths; k++) {
            //     if (strcmp(peer_strings[j], peer_info->file_paths[k]) == 0) {
            //         char field_name[20];
            //         sprintf(field_name, "file_path_%d", k);
            //         cJSON_AddStringToObject(peer, field_name, peer_info->file_paths[k]);
            //     }
            // }
        }
        cJSON_AddItemToArray(peers_array, peer);
    }
    
    if(has_peer_field){
        cJSON_AddItemToObject(json_root, "peers", peers_array);
    }

    char *json = cJSON_Print(json_root);
    cJSON_Delete(json_root);

    printf("json: %s \n", json);

    return json;
}

char* get_all_configs_json(char** config_strings, int config_strings_count, char** peer_strings, int peer_strings_count, hash_table *ht) {
    cJSON *json_root = cJSON_CreateObject();
    printf("config_strings_count: %d \t peer_strings_count: %d \n", config_strings_count, peer_strings_count);
    // Iterate through the buckets in the hashtable
    for (int i = 0; i < ht->size; i++) {
        hash_node *node = ht->buckets[i];

        // Iterate through the nodes in the bucket
        while (node) {
            char *uuid = (char*)node->key;
            node_config *config = (node_config*)node->value;

            // Output the fields for the node_config
            char *config_fields_json = get_config_fields_json(config_strings, config_strings_count, peer_strings, peer_strings_count, config);
            cJSON *config_fields_obj = cJSON_Parse(config_fields_json);
            free(config_fields_json);

            cJSON_AddItemToObject(json_root, uuid, config_fields_obj);

            node = node->next;
        }
    }

    char *json_output = cJSON_PrintUnformatted(json_root);
    // printf("%s\n", json_output);
    // cJSON_free(json_output);
    cJSON_Delete(json_root);
    return json_output;
}


