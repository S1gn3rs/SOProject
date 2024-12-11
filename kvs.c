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
    //Problema se estiver mais que um a tentar criar o segundo vai perder ponteiro correto !!!!!!!!!!!!//////////////////777
    //if (pthread_rwlock_wrlock_error_check(&ht->rwl, NULL) != 0) return 1;
    IndexList *indexList = ht->table[index];

    //pthread_rwlock_unlock(&ht->rwl);

    //if (pthread_rwlock_rdlock_error_check(&ht->rwl, NULL) != 0) return 1;

    //if (pthread_rwlock_wrlock_error_check(&indexList->rwl, &ht->rwl)) return 1;

    keyNode = indexList->head;
    // Search for the key node
    while (keyNode != NULL) {

        if (strcmp(keyNode->key, key) == 0) {
            free(keyNode->value);
            keyNode->value = strdup(value);
            //pthread_rwlock_unlock(&indexList->rwl);
            //pthread_rwlock_unlock(&ht->rwl);
            return 0;
        }
        keyNode = keyNode->next; // Move to the next node
    }

    newKeyNode = malloc(sizeof(KeyNode));
    newKeyNode->key = strdup(key);
    newKeyNode->value = strdup(value);
    newKeyNode->next = (indexList->head != NULL)? indexList->head->next : NULL;
    indexList->head = newKeyNode;

    //pthread_rwlock_unlock(&indexList->rwl);
    //pthread_rwlock_unlock(&ht->rwl);
    return 0;
}


char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    KeyNode *keyNode;
    char* value;
    if (pthread_rwlock_rdlock_error_check(&ht->rwl, NULL) != 0) return NULL;

    IndexList *indexList = ht->table[index];

    if (pthread_rwlock_rdlock_error_check(&indexList->rwl, &ht->rwl)) return NULL;
    keyNode = indexList->head;

    while (keyNode != NULL) {
        if (strcmp(keyNode->key, key) == 0) {
            value = strdup(keyNode->value);
            pthread_rwlock_unlock(&indexList->rwl);
            pthread_rwlock_unlock(&ht->rwl);
            return value; // Return copy of the value if found
        }
        keyNode = keyNode->next; // Move to the next node
    }

    pthread_rwlock_unlock(&indexList->rwl);
    pthread_rwlock_unlock(&ht->rwl);
    return NULL; // Key not found
}


int delete_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    IndexList *indexList;
    KeyNode *keyNode, *prevNode = NULL;
    if (pthread_rwlock_rdlock_error_check(&ht->rwl, NULL)) return 1;
    indexList = ht->table[index];
    if (pthread_rwlock_wrlock_error_check(&indexList->rwl, &ht->rwl)) return 1;
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
                free(keyNode); // Free the key node itself
            }
            else{
                keyNode->key = strdup("-");
                keyNode->value = strdup("-");
            }
            pthread_rwlock_unlock(&indexList->rwl);
            pthread_rwlock_unlock(&ht->rwl);
            return 0; // Exit the function
        }
        prevNode = keyNode; // Move prevNode to current node
        keyNode = keyNode->next; // Move to the next node
    }
    pthread_rwlock_unlock(&indexList->rwl);
    pthread_rwlock_unlock(&ht->rwl);
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

// void free_table(HashTable *ht){
//     pthread_t threads[TABLE_SIZE];
//     for (int i = 0; i < TABLE_SIZE; i++){
//         if (pthread_create(&threads[i], NULL, thread_clean_list, (void *) ht->table[i]) != 0){
//             fprintf()
//         }
//     }
//     for (int i = 0; i < TABLE_SIZE; i++){
//         pthread_join(threads[i], NULL);
//     }
// }


// void *thread_clean_list(void *args){
//     IndexList *indexList = (IndexList*) args;
//     if (indexList == NULL) return NULL;
//         KeyNode *keyNode = indexList->head;
//         while (keyNode != NULL) {
//             KeyNode *temp = keyNode;
//             keyNode = keyNode->next;
//             free(temp->key);
//             free(temp->value);
//             free(temp);
//         }
//     pthread_rwlock_destroy(&indexList->rwl);
//     free(indexList);
//     return NULL;
// }