/*
 * TRIE DATA STRUCTURE
 * Efficient file path lookup data structure providing O(m) search time where m is path length
 * Uses 256-ary tree (one child per ASCII character) for optimal character-by-character traversal
 * Each file path is stored as a sequence of characters leading to a leaf node containing SS_ID
 * This enables fast routing of file requests to the correct Storage Server
 */

#include "../include/ns_storage.h"
#include "../../common/include/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern Logger *global_logger;

/* Create a new Trie node with 256 children (one for each ASCII character) */
TrieNode *trie_create_node()
{
    TrieNode *node = (TrieNode *)calloc(1, sizeof(TrieNode));
    node->is_end_of_file = 0;
    node->file_name = NULL;
    node->ss_count = 0;
    for (int i = 0; i < MAX_SS_PER_FILE; i++)
    {
        node->storage_server_ids[i] = -1;
    }
    return node;
}

/* Insert a file path into Trie and associate it with a Storage Server ID
 * Traverses character by character, creating nodes as needed
 * Marks the final node as end of file and stores the SS_ID for routing
 * Supports multiple SS_IDs per file for replication
 */
void trie_insert(TrieNode *root, const char *filename, int ss_id)
{
    TrieNode *p = root;
    for (int i = 0; filename[i]; ++i)
    {
        unsigned char idx = filename[i];
        if (!p->children[idx])
            p->children[idx] = trie_create_node();
        p = p->children[idx];
    }
    p->is_end_of_file = 1;
    if (!p->file_name)
    {
        p->file_name = strdup(filename);
    }
    
    // Add SS_ID if not already present
    int found = 0;
    for (int i = 0; i < p->ss_count; i++)
    {
        if (p->storage_server_ids[i] == ss_id)
        {
            found = 1;
            break;
        }
    }
    
    if (!found && p->ss_count < MAX_SS_PER_FILE)
    {
        p->storage_server_ids[p->ss_count++] = ss_id;
    }
}

/* Search for a file in the Trie and retrieve its Storage Server ID
 * Returns 1 if file found, 0 if not found
 * If found, outputs the first SS_ID through ss_id_out pointer for routing purposes
 * Time complexity: O(m) where m is the length of the filename
 */
int trie_search(TrieNode *root, const char *filename, int *ss_id_out)
{
    TrieNode *p = root;
    for (int i = 0; filename[i]; ++i)
    {
        unsigned char idx = filename[i];
        if (!p->children[idx])
            return 0;
        p = p->children[idx];
    }
    if (p && p->is_end_of_file && p->ss_count > 0)
    {
        if (ss_id_out)
            *ss_id_out = p->storage_server_ids[0];
        return 1;
    }
    return 0;
}

/* Search for a file and get all SS_IDs that store it
 * Returns the number of SS_IDs found
 */
int trie_search_all(TrieNode *root, const char *filename, int *ss_ids_out, int max_ids)
{
    TrieNode *p = root;
    for (int i = 0; filename[i]; ++i)
    {
        unsigned char idx = filename[i];
        if (!p->children[idx])
            return 0;
        p = p->children[idx];
    }
    
    if (p && p->is_end_of_file)
    {
        int count = (p->ss_count < max_ids) ? p->ss_count : max_ids;
        for (int i = 0; i < count; i++)
        {
            ss_ids_out[i] = p->storage_server_ids[i];
        }
        return count;
    }
    
    return 0;
}

static int trie_has_children(TrieNode *node)
{
    for (int i = 0; i < 256; i++)
    {
        if (node->children[i])
            return 1;
    }
    return 0;
}

static int trie_remove_recursive(TrieNode *node, const char *filename, int depth)
{
    if (!node)
        return 0;

    if (filename[depth] == '\0')
    {
        if (!node->is_end_of_file)
            return 0;

        node->is_end_of_file = 0;
        node->ss_count = 0;
        for (int i = 0; i < MAX_SS_PER_FILE; i++)
        {
            node->storage_server_ids[i] = -1;
        }
        if (node->file_name)
        {
            free(node->file_name);
            node->file_name = NULL;
        }

        return trie_has_children(node) ? 0 : 1;
    }

    unsigned char idx = (unsigned char)filename[depth];
    if (!node->children[idx])
        return 0;

    int should_delete_child = trie_remove_recursive(node->children[idx], filename, depth + 1);
    if (should_delete_child)
    {
        free(node->children[idx]);
        node->children[idx] = NULL;
    }

    if (node->is_end_of_file || node->file_name)
        return 0;

    return trie_has_children(node) ? 0 : 1;
}

void trie_remove(TrieNode *root, const char *filename)
{
    if (!root || !filename)
        return;

    trie_remove_recursive(root, filename, 0);
}

/* Remove a specific SS_ID from a file's replication list */
void trie_remove_ss(TrieNode *root, const char *filename, int ss_id)
{
    if (!root || !filename)
        return;
    
    TrieNode *p = root;
    for (int i = 0; filename[i]; ++i)
    {
        unsigned char idx = filename[i];
        if (!p->children[idx])
            return;
        p = p->children[idx];
    }
    
    if (!p || !p->is_end_of_file)
        return;
    
    // Find and remove the SS_ID
    for (int i = 0; i < p->ss_count; i++)
    {
        if (p->storage_server_ids[i] == ss_id)
        {
            // Shift remaining IDs
            for (int j = i; j < p->ss_count - 1; j++)
            {
                p->storage_server_ids[j] = p->storage_server_ids[j + 1];
            }
            p->storage_server_ids[p->ss_count - 1] = -1;
            p->ss_count--;
            
            // If no more SS have this file, remove it completely
            if (p->ss_count == 0)
            {
                trie_remove(root, filename);
            }
            break;
        }
    }
}

/* ========== TRIE PERSISTENCE FUNCTIONS ========== */

#define TRIE_FILE "name_server/data/trie.dat"

/* Helper: Recursively collect all files from Trie */
static void collect_files_recursive(TrieNode *node, char files[][256], int ss_ids[], int *count, int max_count)
{
    if (!node || *count >= max_count)
        return;

    // If this is end of a file path, add to collection
    if (node->is_end_of_file && node->file_name && node->ss_count > 0)
    {
        strncpy(files[*count], node->file_name, 255);
        files[*count][255] = '\0';
        ss_ids[*count] = node->storage_server_ids[0]; // Use first SS ID for persistence
        (*count)++;
    }

    // Recursively traverse all children
    for (int i = 0; i < 256; i++)
    {
        if (node->children[i])
        {
            collect_files_recursive(node->children[i], files, ss_ids, count, max_count);
        }
    }
}

/* Collect all files from Trie for persistence */
void trie_get_all_files(TrieNode *root, char files[][256], int ss_ids[], int *count, int max_count)
{
    *count = 0;
    if (root)
    {
        collect_files_recursive(root, files, ss_ids, count, max_count);
    }

    if (global_logger)
    {
        log_message(global_logger, LOG_DEBUG, "Collected %d files from Trie", *count);
    }
}

/* Save Trie to disk (Day 13) */
int save_trie_to_disk(TrieNode *root)
{
    if (!root)
    {
        if (global_logger)
        {
            log_message(global_logger, LOG_ERROR, "Cannot save NULL Trie");
        }
        return -1;
    }

    FILE *fp = fopen(TRIE_FILE, "wb");
    if (!fp)
    {
        if (global_logger)
        {
            log_message(global_logger, LOG_ERROR, "Failed to open Trie file for writing");
        }
        return -1;
    }

    // Collect all files from Trie
    char files[512][256];
    int ss_ids[512];
    int count = 0;

    trie_get_all_files(root, files, ss_ids, &count, 512);

    // Write count
    fwrite(&count, sizeof(int), 1, fp);

    // Write each file path and SS ID
    for (int i = 0; i < count; i++)
    {
        int path_len = strlen(files[i]) + 1;
        fwrite(&path_len, sizeof(int), 1, fp);
        fwrite(files[i], 1, path_len, fp);
        fwrite(&ss_ids[i], sizeof(int), 1, fp);
    }

    fclose(fp);

    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "Saved %d file mappings to disk", count);
    }

    return 0;
}

/* Load Trie from disk (Day 13) */
int load_trie_from_disk(TrieNode *root)
{
    if (!root)
    {
        if (global_logger)
        {
            log_message(global_logger, LOG_ERROR, "Cannot load into NULL Trie");
        }
        return -1;
    }

    FILE *fp = fopen(TRIE_FILE, "rb");
    if (!fp)
    {
        if (global_logger)
        {
            log_message(global_logger, LOG_INFO, "No Trie file found, starting fresh");
        }
        return 0; // Not an error - just starting fresh
    }

    // Read count
    int count;
    if (fread(&count, sizeof(int), 1, fp) != 1)
    {
        if (global_logger)
        {
            log_message(global_logger, LOG_ERROR, "Failed to read Trie count");
        }
        fclose(fp);
        return -1;
    }

    // Validate count
    if (count < 0 || count > 512)
    {
        if (global_logger)
        {
            log_message(global_logger, LOG_ERROR, "Invalid Trie count: %d", count);
        }
        fclose(fp);
        return -1;
    }

    // Read and insert each file
    for (int i = 0; i < count; i++)
    {
        int path_len;
        char path[256];
        int ss_id;

        if (fread(&path_len, sizeof(int), 1, fp) != 1 ||
            path_len <= 0 || path_len > 256)
        {
            if (global_logger)
            {
                log_message(global_logger, LOG_ERROR, "Invalid path length at entry %d", i);
            }
            fclose(fp);
            return -1;
        }

        if (fread(path, 1, path_len, fp) != path_len ||
            fread(&ss_id, sizeof(int), 1, fp) != 1)
        {
            if (global_logger)
            {
                log_message(global_logger, LOG_ERROR, "Failed to read Trie entry %d", i);
            }
            fclose(fp);
            return -1;
        }

        // Insert into Trie
        trie_insert(root, path, ss_id);
    }

    fclose(fp);

    if (global_logger)
    {
        log_message(global_logger, LOG_INFO, "Loaded %d file mappings from disk", count);
    }

    return 0;
}
