#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#define TABLE_SIZE 26

#include <stddef.h>
#include <pthread.h>

#include "avl.h"

/// Node of the linked list.
typedef struct KeyNode {
    char *key; // Key of the pair.
    char *value; // Value of the pair.
    struct KeyNode *next; // Pointer to the next node.
    struct AVL *avl_notif_fds; // AVL tree for client's notification fd.
} KeyNode;

/// List of key value pairs.
typedef struct IndexList {
    KeyNode *head; // Pointer to the first node.
    pthread_rwlock_t rwl; // Read-write lock.
} IndexList;

/// Hash table structure.
typedef struct HashTable {
    IndexList *table[TABLE_SIZE]; // Array of linked lists.
    pthread_rwlock_t rwl; // Read-write lock.
} HashTable;

/// Hash function to calculate the index of the key.
/// @param key
/// @return Index of the key.
int hash(const char *key);

/// Creates a new event hash table.
/// @return Newly created hash table, NULL on failure
HashTable *create_hash_table();

/// Appends a new key value pair to the hash table.
/// @param ht Hash table to be modified.
/// @param key Key of the pair to be written.
/// @param value Value of the pair to be written.
/// @return 0 if the node was appended successfully, -1 otherwise.
int write_pair(HashTable *ht, const char *key, const char *value);

/// Deletes the value of given key.
/// @param ht Hash table to delete from.
/// @param key Key of the pair to be deleted.
/// @return 0 if the node was deleted successfully, -1 otherwise.
char* read_pair(HashTable *ht, const char *key);

/// Appends a new node to the list.
/// @param list Event list to be modified.
/// @param key Key of the pair to read.
/// @return 0 if the node was appended successfully, -1 otherwise.
int delete_pair(HashTable *ht, const char *key);

/// Frees the hashtable.
/// @param ht Hash table to be deleted.
void free_table(HashTable *ht);

#endif  // KVS_H
