#ifndef KEY_VALUE_STORE_H
#define KEY_VALUE_STORE_H

#define TABLE_SIZE 26

#include <stddef.h>
#include <pthread.h>

typedef struct KeyNode {
    char *key;
    char *value;
    pthread_rwlock_t rwl;
    struct KeyNode *next;
} KeyNode;

typedef struct IndexList {
    KeyNode *head;
    pthread_rwlock_t rwl;
} IndexList;

typedef struct HashTable {
    IndexList *table[TABLE_SIZE];
    pthread_mutex_t mutex;
} HashTable;

int pthread_rwlock_rdlock_error_check(pthread_rwlock_t *rwlock, pthread_rwlock_t *toUnlock);

int pthread_rwlock_wrlock_error_check(pthread_rwlock_t *rwlock, pthread_rwlock_t *toUnlock);

/// Creates a new event hash table.
/// @return Newly created hash table, NULL on failure
HashTable *create_hash_table();

/// Appends a new key value pair to the hash table.
/// @param ht Hash table to be modified.
/// @param key Key of the pair to be written.
/// @param value Value of the pair to be written.
/// @return 0 if the node was appended successfully, 1 otherwise.
int write_pair(HashTable *ht, const char *key, const char *value);

/// Deletes the value of given key.
/// @param ht Hash table to delete from.
/// @param key Key of the pair to be deleted.
/// @return 0 if the node was deleted successfully, 1 otherwise.
char* read_pair(HashTable *ht, const char *key);

/// Appends a new node to the list.
/// @param list Event list to be modified.
/// @param key Key of the pair to read.
/// @return 0 if the node was appended successfully, 1 otherwise.
int delete_pair(HashTable *ht, const char *key);

/// Frees the hashtable.
/// @param ht Hash table to be deleted.
void free_table(HashTable *ht);


#endif  // KVS_H
