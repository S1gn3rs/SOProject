#include "safeFunctions.h"

void safe_strncpy(char *dest, const char *src, size_t dest_size) {
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0'; // Ensure null-termination
}


int pthread_rwlock_rdlock_error_check(pthread_rwlock_t *rwlock, \
    pthread_rwlock_t *to_unlock){

    if (pthread_rwlock_rdlock(rwlock) != 0) {
        // if first unlock gets an error unlock toUnlock
        if (to_unlock != NULL) pthread_rwlock_unlock(to_unlock);
        fprintf(stderr, "Error locking rwl to read\n");
        return -1;
    }
    return 0;
}


int pthread_rwlock_wrlock_error_check(pthread_rwlock_t *rwlock, \
    pthread_rwlock_t *to_unlock){

    if (pthread_rwlock_wrlock(rwlock) != 0) {
        if (to_unlock != NULL) pthread_rwlock_unlock(to_unlock);
        fprintf(stderr, "Error locking rwl to write\n");
        return -1;
    }
    return 0;
}


char* strdup_error_check(const char * bufferToCopy){
    char * new_buffer = strdup(bufferToCopy);
    if (new_buffer == NULL){
      fprintf(stderr, "Error trying to duplicate string.\n");
    }
    return new_buffer;
}
