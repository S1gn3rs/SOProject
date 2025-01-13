#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#define TABLE_SIZE 26

#include <stddef.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include "operations.h"
#include "constants.h"
#include "../common/constants.h"
#include "avl.h"
#include "../common/safeFunctions.h"


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


typedef struct AVLSessions {
  struct AVL *avl_clients_node[MAX_SESSION_COUNT];
  int num_subs[MAX_SESSION_COUNT];
  // Mutexes are needed because of deletes, (only the dec_num_subs needs).
  pthread_mutex_t session_mutex[MAX_SESSION_COUNT];
}AVLSessions;


/**
 * Hash function to calculate the index of the key.
 *
 * @param key The key to hash.
 * @return Index of the key.
 */
int hash(const char *key);


/**
 * Creates a new hash table.
 *
 * @return Newly created hash table, NULL on failure.
 */
HashTable *create_hash_table();


/**
 * Appends a new key value pair to the hash table.
 *
 * @param ht Hash table to be modified.
 * @param key Key of the pair to be written.
 * @param value Value of the pair to be written.
 * @param notif_message String to write to all fd stores inside the node.
 * @return 0 if the node was appended successfully, -1 otherwise.
 */
int write_pair(HashTable *ht, const char *key, const char *value,\
    const char *notif_message);


/**
 * Deletes the value of given key.
 *
 * @param ht Hash table to read from.
 * @param key Key of the pair to read.
 * @return 0 if the node was deleted successfully, -1 otherwise.
 */
char* read_pair(HashTable *ht, const char *key);


/**
 * Appends a new node to the list.
 *
 * @param ht Hash table to delete from.
 * @param avl_sessions List of all AVL tree subscriptions for all clients.
 * @param key Key of the pair to be deleted.
 * @param notif_message String to write to all fd stores inside the node.
 * @return 0 if the node was appended successfully, -1 otherwise.
 */
int delete_pair(HashTable *ht, AVLSessions *avl_sessions, const char *key,\
    const char *notif_message);


/**
 * Subscribes a client to a key.
 *
 * @param ht Hash table to modify.
 * @param key Key to subscribe to.
 * @param session_id ID of the current session.
 * @param notif_fd File descriptor for notifications.
 * @return 0 on success, -1 otherwise.
 */
int subscribe_pair(HashTable *ht, char key[MAX_STRING_SIZE + 1],\
    int session_id, int notif_fd);


/**
 * Unsubscribes a client from a key.
 *
 * @param ht Hash table to modify.
 * @param key Key to unsubscribe from.
 * @param session_id ID of the current session.
 * @return 0 on success, -1 otherwise.
 */
int unsubscribe_pair(HashTable *ht, char key[MAX_STRING_SIZE + 1],\
    int session_id);


/**
 * Frees the hashtable.
 *
 * @param ht Hash table to be deleted.
 */
void free_table(HashTable *ht);


#endif  // KVS_H
