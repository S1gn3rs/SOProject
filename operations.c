#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>


#include "kvs.h"
#include "constants.h"

/// Hash table to store the key value pairs.
static struct HashTable* kvs_table = NULL;


/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}


int kvs_init() {
  if (kvs_table != NULL) {
    fprintf(stderr, "KVS state has already been initialized\n");
    return 1;
  }

  kvs_table = create_hash_table();
  return kvs_table == NULL;index
}


int kvs_terminate() {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }
  free_table(kvs_table);
  return 0;
}


/// Activate kvs hash table's lock to write and checks if it gets any error, if
/// an error ocurred reports it and won't execute the lock.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_wrlock(){
  return pthread_rwlock_wrlock_error_check(&kvs_table->rwl, NULL);
}


/// Activate kvs hash table's lock to read and checks if it gets any error, if
/// an error ocurred reports it and won't execute the lock.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_rdlock(){
  return pthread_rwlock_rdlock_error_check(&kvs_table->rwl, NULL);
}


/// Desactivate kvs hash table's lock (unlock) and checks if it gets any error,
/// if an error ocurred reports it and won't execute the unlock.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_unlock(){
  return pthread_rwlock_unlock(&kvs_table->rwl);
}


/// Activate lock to write of hash table's index list and checks if it gets any
/// error, if an error ocurred reports it and won't execute the lock.
/// @param index To get the corresponding list of that index in the hash table.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_list_wrlock(size_t index){
  return pthread_rwlock_wrlock_error_check(&kvs_table->table[index]->rwl, &kvs_table->rwl);
}


/// Desactivate lock (unlock) of hash table's index list and checks if it gets
/// any error, if an error ocurred reports it and won't execute the unlock.
/// @param index To get the corresponding list of that index in the hash table.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_list_unlock(size_t index){
  return pthread_rwlock_unlock(&kvs_table->table[index]->rwl);
}


/// A variation of insertion sort that sorts an index list based on it's keys.
/// @param indexs List of indexs to be sorted.
/// @param num_pairs Amount of indexs that need to be sorted by their key.
/// @param keys Correspondent key for each index.
void insertion_sort(size_t *indexs, size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  for (size_t i = 1; i < num_pairs; i++) {
    size_t current = indexs[i];
    size_t j = i;

    while (j > 0 && strcasecmp(keys[indexs[j - 1]], keys[current]) > 0) {
      indexs[j] = indexs[j - 1];
      j--;
    }

    indexs[j] = current;
  }
}

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]) {
  int isLocked[26] = {0}; // To indicate witch indices have been locked

  // Check if the KVS state has been initialized
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  // Allocate memory for the indexs
  size_t *indexs = malloc(num_pairs * sizeof(size_t));
  // Check if the memory was allocated successfully
  if (indexs == NULL) {
    fprintf(stderr, "Failed to allocate memory for index\n");
    return 1;
  }

  // Fill the indexs with the indexs of the pairs
  for (size_t i = 0; i < num_pairs; i++) {
    indexs[i] = i;
  }

  // Lock the hash table for reading
  hash_table_rdlock();

  // Sort the indexs based on the keys
  insertion_sort(indexs, num_pairs, keys);

  // Iterate over the pairs
  for (size_t i = 0; i < num_pairs; i++) {
    size_t indexNodes = indexs[i];
    size_t indexList = (size_t) hash(keys[indexNodes]);
    if (isLocked[indexList] == 0){
      isLocked[indexList] = 1;
      // Lock the index list for writing
      hash_table_list_wrlock(indexList);
    }
    // Try to write the key value pair to the hash table
    if (write_pair(kvs_table, keys[indexNodes], values[indexNodes]) != 0) {

      fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[indexNodes], values[indexNodes]);
    }
  }

  // Unlock the hash table's index list that have been locked
  for (size_t ind = 0; ind < 26; ind++)
    if (isLocked[ind]) hash_table_list_unlock(ind);

  // Unlock the hash table
  hash_table_unlock();
  // Free the memory allocated for the indexs
  free(indexs);
  return 0;
}


int kvs_read(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  // Check if the KVS state has been initialized
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  // Buffer to store the read content
  char buffer[MAX_WRITE_SIZE];
  int length;

  if (write(fd, "[", 1) == -1) {
      perror("Error writing\n");
      return 1;
  }

  size_t *indexs = malloc(num_pairs * sizeof(size_t));
  if (indexs == NULL) {
    fprintf(stderr, "Failed to allocate memory for index\n");
    return 1;
  }

  for (size_t i = 0; i < num_pairs; i++) {
    indexs[i] = i;
  }

  insertion_sort(indexs, num_pairs, keys);

  for (size_t i = 0; i < num_pairs; i++) {
    size_t index = indexs[i];
    char* result = read_pair(kvs_table, keys[index]);
    if (result == NULL) {
      length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,KVSERROR)", keys[index]);
      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        free(indexs);
        return 1;
      }
    } else {
      length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,%s)", keys[index], result);
      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        free(indexs);
        return 1;
      }
    }
    free(result);
  }
  if (write(fd, "]\n", 2) == -1) {
    perror("Error writing\n");
    free(indexs);
    return 1;
  }
  free(indexs);
  return 0;
}

int kvs_delete(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  // Check if the KVS state has been initialized
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }
  int aux = 0;
  
  char buffer[MAX_WRITE_SIZE];
  for (size_t i = 0; i < num_pairs; i++) {
    if (delete_pair(kvs_table, keys[i]) != 0) {
      int length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,KVSMISSING)", keys[i]);
      if (!aux) {
        if (write(fd, "[", 1) == -1) {
          perror("Error writing\n");
          return 1;
        }
        aux = 1;
      }
      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        return 1;
      }
    }
  }
  if (aux) {
    if (write(fd, "]\n", 2) == -1) {
      perror("Error writing\n");
      return 1;
    }
  }

  return 0;
}

int kvs_show(int fd) {
  // Try to lock the hash table for writing
  if (pthread_rwlock_wrlock_error_check(&kvs_table->rwl, NULL) != 0) return 1;

  // Iterate over the hash table
  for (int i = 0; i < TABLE_SIZE; i++) {
    // Get the index list
    IndexList *indexList = kvs_table->table[i];
    KeyNode *keyNode; //*tempNode;

    // If the index list is NULL, continue to the next index
    if (indexList == NULL) continue;
    // if (pthread_rwlock_rdlock_error_check(&indexList->rwl, NULL)) return 1;

    // Get the first key node
    keyNode = indexList->head;

    // if(keyNode == NULL || pthread_rwlock_rdlock_error_check(&keyNode->rwl, &indexList->rwl)) return 1;
    if (keyNode != NULL && strcmp(keyNode->key, "-") == 0) keyNode = keyNode->next;//////////////////////////////////////////////
    // pthread_rwlock_unlock(&keyNode->rwl);

    // Buffer to store the show content
    char buffer[MAX_WRITE_SIZE];
    // Iterate over the key nodes
    while (keyNode != NULL) {

      // Get the length of the key value pair
      int length = snprintf(buffer, MAX_WRITE_SIZE, "(%s, %s)\n", keyNode->key, keyNode->value);

      //Try to write the key value pair to the file descriptor
      if (write(fd, buffer, (size_t)length) == -1) { /////////////////////////atomico?
        pthread_rwlock_unlock(&kvs_table->rwl);
        perror("Error writing\n");
        return 1;
      }
      keyNode = keyNode->next; // Move to the next node
    }
  }
  // Unlock the hash table
  pthread_rwlock_unlock(&kvs_table->rwl);
  return 0;
}

int kvs_backup(int fd) {
  // Buffer to store the backup content
  char buffer[MAX_WRITE_SIZE];

  // Iterate over the hash table
  for (int i = 0; i < TABLE_SIZE; i++) {
    // Get the index list
    IndexList *indexList = kvs_table->table[i];
    KeyNode *keyNode;

    // If the index list is NULL, continue to the next index
    if (indexList == NULL) continue;

    keyNode = indexList->head;

    if (keyNode != NULL && strcmp(keyNode->key, "-") == 0) keyNode = keyNode->next; /////////////////////////////////////////////////////////////////////////////////

    // Iterate over the key nodes
    while (keyNode != NULL) {
      char *key = keyNode->key;
      char *value = keyNode->value;
      char *buf_ptr = buffer;

      // Write the key value pair to the buffer
      *buf_ptr++ = '(';

      while(*key && buf_ptr < buffer + MAX_WRITE_SIZE - 4) *buf_ptr++ = *key++;

      *buf_ptr++ = ',';
      *buf_ptr++ = ' ';

      while(*value && buf_ptr < buffer + MAX_WRITE_SIZE - 2) *buf_ptr++ = *value++;

      *buf_ptr++ = ')';
      *buf_ptr++ = '\n';

      // Calculate the length of the buffer
      size_t length = (size_t)(buf_ptr - buffer);

      // Write the buffer to the file descriptor
      if (write(fd, buffer, (size_t)length) == -1) return 1; ////////////////////////////////////////////backup not atomic???

      // Move to the next node
      keyNode = keyNode->next;
    }
  }
  return 0;
}

void kvs_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}

/// Do a safe fork preventing the child process to have any locks on the hashTable
/// @return value of the child's pid to parent process and 0 to child process
pid_t do_fork(){
  hash_table_wrlock();
  pid_t pid = fork();
  hash_table_unlock();
  return pid;
}
