#include "kvs.h"
#include "string.h"
#include <stdio.h>

#include <stdlib.h>
#include <ctype.h>


// Hash function based on key initial.
// @param key Lowercase alphabetical string.
// @return hash.
// NOTE: This is not an ideal hash function, but is useful for test purposes of the project
int hash(const char *key) {
    int firstLetter = tolower(key[0]);
    if (firstLetter >= 'a' && firstLetter <= 'z') {
        return firstLetter - 'a';
    } else if (firstLetter >= '0' && firstLetter <= '9') {
        return firstLetter - '0';
    }
    return -1; // Invalid index for non-alphabetic or number strings
}


HashTable* create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht) return NULL;
  for (int i = 0; i < TABLE_SIZE; i++) {
      ht->table[i] = NULL;
  }
  return ht;
}

IndexList* create_IndexList(){
    IndexList *indexList = malloc(sizeof(IndexList));
    if (!indexList) return NULL;
    indexList->head = NULL;
    if(pthread_rwlock_init(&indexList->rwl, NULL) != 0) {
        return NULL;
    }
    return indexList;
}

int write_pair(HashTable *ht, const char *key, const char *value) {
    int index = hash(key);
    IndexList *indexList = ht->table[index];
    KeyNode *keyNode;
    if (indexList == NULL) {
        ht->table[index] = create_IndexList();
        indexList = ht->table[index];
    }
    if (pthread_rwlock_wrlock(&indexList->rwl) != 0) {
        fprintf(stderr, "Error locking list rwl\n");
        return 1;
    }
    keyNode = indexList->head;
    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            if (pthread_rwlock_wrlock(&indexList->rwl) != 0) {
                fprintf(stderr, "Error locking list rwl\n");
                return 1;
            }
            free(keyNode->value);
            keyNode->value = strdup(value);
            pthread_rwlock_unlock(&keyNode->rwl);
            pthread_rwlock_unlock(&indexList->rwl);
            return 0;
        }
        keyNode = keyNode->next; // Move to the next node
    }

    // Key not found, create a new key node
    keyNode = malloc(sizeof(KeyNode));
    keyNode->key = strdup(key); // Allocate memory for the key
    keyNode->value = strdup(value); // Allocate memory for the value
    // if(pthread_rwlock_init(&keyNode->rwl, NULL) != 0) {
        // free(keyNode->key);
        // free(keyNode->value);
        // free(keyNode);
        // return 1;
    // }
    keyNode->next = indexList->head; // Link to existing nodes
    indexList->head = keyNode; // Place new key node at the start of the list

    pthread_rwlock_unlock(&indexList->rwl);
    return 0;
}

char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    IndexList *indexList = ht->table[index];
    KeyNode *keyNode;
    char* value;

    if (indexList == NULL) return NULL;
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
    IndexList *indexList = ht->table[index];
    KeyNode *keyNode = indexList->head;
    KeyNode *prevNode = NULL;

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
        free(indexList);
    }
    free(ht);
}