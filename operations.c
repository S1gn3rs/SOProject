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

int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], char values[][MAX_STRING_SIZE]) {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return 1;
  }

  for (size_t i = 0; i < num_pairs; i++) {
    if (write_pair(kvs_table, keys[i], values[i]) != 0) {
      fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[i], values[i]);
    }
  }

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

  for (size_t i = 0; i < num_pairs; i++) {
    char* result = read_pair(kvs_table, keys[i]);
    if (result == NULL) {
      length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,KVSERROR)", keys[i]);
      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        return 1;
      }
    } else {
      length = snprintf(buffer, MAX_WRITE_SIZE, "(%s,%s)", keys[i], result);
      if (write(fd, buffer, (size_t)length) == -1) {
        perror("Error writing\n");
        return 1;
      }
    }
    free(result);
  }
  if (write(fd, "]\n", 2) == -1) {
    perror("Error writing\n");
    return 1;
  }

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
    KeyNode *keyNode = kvs_table->table[i];
    
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

int kvs_backup() {
  return 0;
}

void kvs_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
