#ifndef SAFE_FUNCTIONS_H
#define SAFE_FUNCTIONS_H

#include <string.h>
#include <pthread.h>
#include <stdio.h>

/**
 * Copies up to `dest_size - 1` characters from the string `src` to `dest`,
 * ensuring that the destination string is null-terminated.
 *
 * @param dest The destination buffer where the content is to be copied.
 * @param src The source string to be copied.
 * @param dest_size The size of the destination buffer.
 *
 * @return void
 */
void safe_strncpy(char *dest, const char *src, size_t dest_size);


/// Tries to lock a rwlock in read mode and checks for errors.
/// @param rwlock the rwlock to lock.
/// @param toUnlock in case of error, unlock this rwlock.
/// @return 0 if the lock was successful, -1 otherwise.
int pthread_rwlock_rdlock_error_check(pthread_rwlock_t *rwlock, pthread_rwlock_t *to_unlock);


/// Tries to lock a rwlock in write mode and checks for errors.
/// @param rwlock the rwlock to lock.
/// @param toUnlock in case of error, unlock this rwlock.
/// @return 0 if the lock was successful, -1 otherwise.
int pthread_rwlock_wrlock_error_check(pthread_rwlock_t *rwlock, pthread_rwlock_t *to_unlock);


/// Tries to duplicate a string and checks for errors.
/// @param bufferToCopy string to be duplicated.
/// @return a new duplicated string in success or null pointer otherwise.
char* strdup_error_check(const char * bufferToCopy);


#endif // SAFE_FUNCTIONS_H