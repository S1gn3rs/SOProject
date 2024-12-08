#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>


#include "kvs.h"
#include "constants.h"

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
  return kvs_table == NULL;
}

int kvs_terminate() {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }
  free_table(kvs_table);
  return 0;
}

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
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
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
    if (write_pair(kvs_table, keys[index], values[index]) != 0) {
      fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[index], values[index]);
    }
  }

  free(indexs);
  return 0;
}


int kvs_read(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

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
    char* result = read_pair(kvs_table, keys[i]);
    if (result == NULL) {
      length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,KVSERROR)", keys[i]);
      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        free(indexs);
        return 1;
      }
    } else {
      length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,%s)", keys[i], result);
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
  for (int i = 0; i < TABLE_SIZE; i++) {
    IndexList *indexList = kvs_table->table[i];
    KeyNode *keyNode;

    if (indexList == NULL) continue;
    keyNode = indexList->head;
    
    char buffer[MAX_WRITE_SIZE];
    while (keyNode != NULL) {
      int length = snprintf(buffer, MAX_WRITE_SIZE, "(%s, %s)\n", keyNode->key, keyNode->value);
      

      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        return 1;
      }
      keyNode = keyNode->next; // Move to the next node
    }
  }
  return 0;
}

int kvs_backup(int fd) {
  for (int i = 0; i < TABLE_SIZE; i++) {
    IndexList *indexList = kvs_table->table[i];
    KeyNode *keyNode;
    
    if (indexList == NULL) continue;
    keyNode = indexList->head;

    char buffer[MAX_WRITE_SIZE];
    while (keyNode != NULL) {
      int length = snprintf(buffer, MAX_WRITE_SIZE, "(%s, %s)\n", keyNode->key, keyNode->value);

      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        return 1;
      }
      keyNode = keyNode->next;
    }
  }
  return 0;
}

void kvs_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
