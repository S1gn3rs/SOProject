#include "kvs.h"
#include <string.h>
#include <stdio.h>

#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>


int strdup_error_check(char *newBuffer, char *bufferToCopy){
    newBuffer = strdup(bufferToCopy);
    if (newBuffer == NULL) {
        fprintf(stderr, "Error trying to do duplicate string.\n");
        return 1;
    }
    return 0;
}

/// Hash function based on key initial.
/// @param key Lowercase alphabetical string.
/// @return hash.
/// NOTE: This is not an ideal hash function, but is useful for test purposes of the project
int hash(const char *key) {
    int firstLetter = tolower(key[0]);
    if (firstLetter >= 'a' && firstLetter <= 'z') {
        return firstLetter - 'a';
    } else if (firstLetter >= '0' && firstLetter <= '9') {
        return firstLetter - '0';
    }
    return -1; // Invalid index for non-alphabetic or number strings
}

/// Function to create a new IndexList.
/// @return a pointer to the new IndexList or NULL on failure.
IndexList* create_IndexList(){
    IndexList *indexList = malloc(sizeof(IndexList));

    if (!indexList) return NULL;
    if (pthread_rwlock_init(&indexList->rwl, NULL) != 0){
        free(indexList);
        return NULL;
    }
    indexList->head = NULL;
    return indexList;
}

HashTable* create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht) return NULL;
  if (pthread_rwlock_init(&ht->rwl, NULL) != 0){
    free(ht);
    return NULL;
  }
  for (int i = 0; i < TABLE_SIZE; i++){
    ht->table[i] = create_IndexList();
  }
  return ht;
}

int pthread_rwlock_rdlock_error_check(pthread_rwlock_t *rwlock, pthread_rwlock_t *toUnlock){
    if (pthread_rwlock_rdlock(rwlock) != 0) {
        if (toUnlock != NULL) pthread_rwlock_unlock(toUnlock); // if first unlock get an error unlock toUnlock
        fprintf(stderr, "Error locking list rwl,\n");
        return 1;
    }
    return 0;
}

int pthread_rwlock_wrlock_error_check(pthread_rwlock_t *rwlock, pthread_rwlock_t *toUnlock){
    if (pthread_rwlock_wrlock(rwlock) != 0) {
        if (toUnlock != NULL) pthread_rwlock_unlock(toUnlock);
        fprintf(stderr, "Error locking list rwl,\n");
        return 1;
    }
    return 0;
}


int write_pair(HashTable *ht, const char *key, const char *value) {
    int index = hash(key);
    KeyNode *keyNode, *newKeyNode;
    IndexList *indexList = ht->table[index];

    keyNode = indexList->head;
    // Search for the key node
    while (keyNode != NULL) {

        if (strcmp(keyNode->key, key) == 0) {
            free(keyNode->value);
            keyNode->value = strdup(value);
            return 0;
        }
        keyNode = keyNode->next; // Move to the next node
    }

    newKeyNode = malloc(sizeof(KeyNode));
    if (!newKeyNode) return 1;
    newKeyNode->key = strdup(key);
    newKeyNode->value = strdup(value);
    newKeyNode->next = (indexList->head != NULL)? indexList->head : NULL;
    indexList->head = newKeyNode;

    return 0;
}


char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode;
    char* value;
    IndexList *indexList = ht->table[index];
    keyNode = indexList->head;

    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            value = strdup(keyNode->value);
            return value; // Return copy of the value if found
        }
        keyNode = keyNode->next; // Move to the next node
    }
    return NULL; // Key not found
}


int delete_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    IndexList *indexList;
    KeyNode *keyNode, *prevNode = NULL;
    indexList = ht->table[index];
    keyNode = indexList->head;

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Key found; delete this node
            if (prevNode == NULL) {
                // Node to delete is the first node in the list
                indexList->head = keyNode->next; // Update the table to point to the next node
            } else {
                // Node to delete is not the first; bypass it
                prevNode->next = keyNode->next; // Link the previous node to the next node
            }
            // Free the memory allocated for the key and value
            free(keyNode->key);
            free(keyNode->value);
            free(keyNode); // Free the key node itself

            return 0; // Exit the function
        }
        prevNode = keyNode; // Move prevNode to current node
        keyNode = keyNode->next; // Move to the next node
    }
    return 1;
}

void free_table(HashTable *ht) {

    for (int i = 0; i < TABLE_SIZE; i++) {
        IndexList *indexList = ht->table[i];
        if (indexList == NULL) continue;
        KeyNode *keyNode = indexList->head;
        while (keyNode != NULL) {
            KeyNode *temp = keyNode;
            keyNode = keyNode->next;
            free(temp->key);
            free(temp->value);
            free(temp);
        }
        pthread_rwlock_destroy(&indexList->rwl);
        free(indexList);
    }
    pthread_rwlock_destroy(&ht->rwl);
    free(ht);
}

int write_error_check(int out_fd, char *buffer) {
  ssize_t total_written = 0;
  size_t done = 0, buff_len = strlen(buffer);

  while ((total_written = write(out_fd, buffer+done, buff_len)) > 0) {
    done += (size_t)total_written;
    buff_len = buff_len - (size_t)total_written;
  }

  if (total_written == -1) {
      fprintf(stderr, "Failed to write to .out file\n");////////////////////////////////////fprintf not signal
      return 1;
  }
  return 0;
}