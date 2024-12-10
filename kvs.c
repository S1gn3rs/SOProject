#include "kvs.h"
#include <string.h>
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
  if (pthread_mutex_init(&ht->mutex, NULL) != 0){
    free(ht);
    return NULL;
  }
  for (int i = 0; i < TABLE_SIZE; i++) ht->table[i] = NULL;
  return ht;
}

IndexList* create_IndexList(const char* key, const char*value){
    IndexList *indexList = malloc(sizeof(IndexList));
    KeyNode *keyNode;

    if (!indexList) return NULL;
    if (pthread_rwlock_init(&indexList->rwl, NULL) != 0){
        free(indexList);
        return NULL;
    }

    keyNode = malloc(sizeof(KeyNode));
    if (!keyNode){
        pthread_rwlock_destroy(&indexList->rwl);
        free(indexList);
        return NULL;
    }
    if (pthread_rwlock_init(&keyNode->rwl, NULL) != 0){
        pthread_rwlock_destroy(&indexList->rwl);
        free(keyNode);
        free(indexList);
        return NULL;
    }
    if (!(keyNode->key = strdup(key))) {
        pthread_rwlock_destroy(&keyNode->rwl);
        pthread_rwlock_destroy(&indexList->rwl);
        free(keyNode);
        free(indexList);
        return NULL;
    }
    if (!(keyNode->value = strdup(value))) {
        free(keyNode->key);
        pthread_rwlock_destroy(&keyNode->rwl);
        pthread_rwlock_destroy(&indexList->rwl);
        free(keyNode);
        free(indexList);
        return NULL;
    }
    keyNode->next = NULL;
    indexList->head = keyNode;
    return indexList;
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

int aux_change_head(KeyNode *baseKeyNode, const char *key, const char *value){
    KeyNode *newKeyNode = baseKeyNode->next;

    if (strcmp(baseKeyNode->value, key) == 0){
        free(baseKeyNode->value);
        baseKeyNode->value = strdup(value);
        return 0;
    }

    if (strcmp(baseKeyNode->key, "-") != 0){
        newKeyNode = malloc(sizeof(KeyNode));
        if (!newKeyNode) return 1;
        if (pthread_rwlock_init(&newKeyNode->rwl, NULL) != 0){
            free(newKeyNode);
            return 1;
        }
        newKeyNode->key = strdup(baseKeyNode->key); // Allocate memory for the key
        newKeyNode->value = strdup(baseKeyNode->value); // Allocate memory for the value
        newKeyNode->next = baseKeyNode->next; // Link to existing nodes
    }
    free(baseKeyNode->key);
    baseKeyNode->key = strdup(key);
    free(baseKeyNode->value);
    baseKeyNode->value = strdup(value);
    baseKeyNode->next = newKeyNode;
    return 0;
}

int write_pair(HashTable *ht, const char *key, const char *value) {
    int index = hash(key);
    KeyNode *keyNode;
    //Problema se estiver mais que um a tentar criar o segundo vai perder ponteiro correto !!!!!!!!!!!!
    if (pthread_mutex_lock(&ht->mutex) != 0) { // COLOCAR EXPLICAÇÃO -----------------------------
        fprintf(stderr, "Error locking table's mutex.\n");
        return 1;
    }
    IndexList *indexList = ht->table[index];
    if (indexList == NULL) {
        ht->table[index] = create_IndexList(key, value);
        if (ht->table[index] == NULL) return 1;
        indexList = ht->table[index];
        pthread_mutex_unlock(&ht->mutex);
        return 0;
    }
    pthread_mutex_unlock(&ht->mutex);

    if (pthread_rwlock_rdlock_error_check(&indexList->rwl, NULL)) return 1;

    if (pthread_rwlock_wrlock_error_check(&indexList->head->rwl, &indexList->rwl)) return 1;
    keyNode = indexList->head;
    if (strcmp(keyNode->key, key) == 0){
        free(keyNode->value);
        keyNode->value = strdup(value);
        pthread_rwlock_unlock(&indexList->head->rwl);
        pthread_rwlock_unlock(&indexList->rwl);
        return 0;
    }
    keyNode = keyNode->next;
    pthread_rwlock_unlock(&indexList->head->rwl);
    // Search for the key node
    while (keyNode != NULL) {

        if (strcmp(keyNode->key, key) == 0) {
            if (pthread_rwlock_wrlock_error_check(&keyNode->rwl, &indexList->rwl)) return 1;

            free(keyNode->value);
            keyNode->value = strdup(value);
            pthread_rwlock_unlock(&keyNode->rwl);
            pthread_rwlock_unlock(&indexList->rwl);
            return 0;
        }
        keyNode = keyNode->next; // Move to the next node
    }

    if (pthread_rwlock_wrlock_error_check(&indexList->head->rwl, NULL)) return 1;
    if (aux_change_head(indexList->head, key, value)) return 1;
    pthread_rwlock_unlock(&indexList->head->rwl);

    pthread_rwlock_unlock(&indexList->rwl);
    return 0;
}


char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode, *nextKeyNode;
    char* value;
    if (pthread_mutex_lock(&ht->mutex) != 0) { // COLOCAR EXPLICAÇÃO -----------------------------
        fprintf(stderr, "Error locking table's mutex.\n");
        return NULL;
    }
    IndexList *indexList = ht->table[index];
    pthread_mutex_unlock(&ht->mutex);

    if (indexList == NULL) return NULL;

    if (pthread_rwlock_rdlock_error_check(&indexList->rwl, NULL)) return NULL;
    keyNode = indexList->head;

    if(keyNode == NULL || pthread_rwlock_rdlock_error_check(&keyNode->rwl, &indexList->rwl)) return NULL;

    if (strcmp(keyNode->key, key) == 0) {
        value = strdup(keyNode->value);
        pthread_rwlock_unlock(&keyNode->rwl);
        pthread_rwlock_unlock(&indexList->rwl);
        return value;
    }
    nextKeyNode = keyNode->next;
    pthread_rwlock_unlock(&keyNode->rwl);
    keyNode = nextKeyNode;

    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            if (pthread_rwlock_rdlock_error_check(&keyNode->rwl, &indexList->rwl)) return NULL;
            value = strdup(keyNode->value);
            pthread_rwlock_unlock(&keyNode->rwl);
            pthread_rwlock_unlock(&indexList->rwl);
            return value; // Return copy of the value if found
        }
        keyNode = keyNode->next; // Move to the next node
    }

    pthread_rwlock_unlock(&indexList->rwl);
    return NULL; // Key not found
}


int delete_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    IndexList *indexList = ht->table[index];
    KeyNode *keyNode, *prevNode = NULL;
    
    if (pthread_rwlock_wrlock_error_check(&indexList->rwl, NULL)) return 1;
    keyNode = indexList->head;

    // Search for the key node
    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            // Key found; delete this node
            if (prevNode == NULL) {
                // Node to delete is the first node in the list
                if(keyNode->next != NULL) indexList->head = keyNode->next; // Update the table to point to the next node
            } else {
                // Node to delete is not the first; bypass it
                prevNode->next = keyNode->next; // Link the previous node to the next node
            }
            // Free the memory allocated for the key and value
            free(keyNode->key);
            free(keyNode->value);
            
            if (keyNode != indexList->head){
                pthread_rwlock_destroy(&keyNode->rwl); // Destroy the rwlock
                free(keyNode); // Free the key node itself
            }
            else{
                keyNode->key = strdup("-");
                keyNode->value = strdup("-");
            }
            pthread_rwlock_unlock(&indexList->rwl);
            return 0; // Exit the function
        }
        prevNode = keyNode; // Move prevNode to current node
        keyNode = keyNode->next; // Move to the next node
    }
    
    pthread_rwlock_unlock(&indexList->rwl);
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
            pthread_rwlock_destroy(&temp->rwl);
            free(temp->key);
            free(temp->value);
            free(temp);
        }
        pthread_rwlock_destroy(&indexList->rwl);
        free(indexList);
    }
    pthread_mutex_destroy(&ht->mutex);
    free(ht);
}
