#ifndef KVS_OPERATIONS_H
#define KVS_OPERATIONS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>
#include <pthread.h>
#include <stddef.h>

#include "avl.h"
#include "kvs.h"
#include "constants.h"
#include "../common/safeFunctions.h"

// Forward declaration of ClientData
typedef struct ClientData ClientData;

/// Initializes the KVS state.
/// @return 0 if the KVS state was initialized successfully, -1 otherwise.
int kvs_init();


/// Destroys the KVS state.
/// @return 0 if the KVS state was terminated successfully, -1 otherwise.
int kvs_terminate();


/// Initializes the AVL sessions state.
/// @return 0 if the AVL sessions state was initialized successfully,
/// -1 otherwise.
int avl_sessions_init();


/// Destroys the AVL sessions state.
/// @return 0 if the AVL sessions state was terminated successfully,
/// -1 otherwise.
int avl_sessions_terminate();


/// Clean the AVL sessions state.
/// @return 0 if the AVL sessions state was cleaned successfully, -1 otherwise.
int avl_clean_sessions();


/// Writes a key value pair to the KVS. If key already exists it is updated.
/// @param num_pairs Number of pairs being written.
/// @param keys Array of keys' strings.
/// @param values Array of values' strings.
/// @return 0 if the pairs were written successfully, -1 otherwise.
int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE],\
    char values[][MAX_STRING_SIZE]);


/// Reads values from the KVS.
/// @param fd File descriptor to write the output.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @param fd File descriptor to write the (successful) output.
/// @return 0 if the key reading, -1 otherwise.
int kvs_read(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]);


/// Deletes key value pairs from the KVS.
/// @param fd File descriptor to write the output.
/// @param num_pairs Number of pairs to read.
/// @param keys Array of keys' strings.
/// @return 0 if the pairs were deleted successfully, -1 otherwise.
int kvs_delete(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]);


/// Writes the state of the KVS.
/// @param fd File descriptor to write the output.
int kvs_show(int fd);


/// Creates a backup of the KVS state and stores it in the correspondent
/// backup file
/// @param fd File descriptor to write the output.
/// @return 0 if the backup was successful, -1 otherwise.
int kvs_backup(int fd);


/// Gets the AVL tree subscritions of a session.
/// @param session_id Session ID.
/// @return AVL tree subscritions of the session.
AVL* get_avl_client(int session_id);


/// Sets the AVL tree subscritions of a session.
/// @param session_id Session ID.
/// @param new_avl AVL tree subscritions to set.
void set_avl_client(int session_id, AVL *new_avl);


/// Sets the AVL client data of a session.
/// @param session_id Session ID.
/// @param new_avl AVL client data to set.
void set_client_info(int session_id, ClientData *new_client_data);


/// Gets the AVL client data of a session.
/// @param session_id Session ID.
/// @return AVL client data of the session.
ClientData* get_client_info(int session_id);

/// Gets the number of subscriptions of a session.
/// @param session_id Session ID.
/// @return Number of subscriptions of the session.
int get_avl_num_subs(int session_id);

/// Gets the mutex of a session.
/// @param session_id Session ID.
/// @return pointer to mutex of the session.
pthread_mutex_t* get_mutex(int sessions_id);


/// Resets the number of subscriptions of a session.
/// @param session_id Session ID.
void reset_num_subs(int session_id);


/// Increment the number of subscriptions of a session.
/// @param session_id Session ID.
void inc_num_subs(int session_id);


/// Decrement the number of subscriptions of a session.
/// @param session_id Session ID.
void dec_num_subs(int session_id);


/// Disconnects a client and removes it from the client AVL tree.
/// @param client_id Client ID.
/// @return 0 if the client was disconnected successfully, -1 otherwise.
int kvs_disconnect(int client_id);


/// Subscribes a client to a key and adds the key to the session AVL tree.
/// @param client_id Client ID.
/// @param notif_fd File descriptor to write the notifications.
/// @param key Key to subscribe to.
/// @return 0 if the client was subscribed successfully, -1 otherwise.
int kvs_subscribe(int client_id, int notif_fd, char *key);


/// Unsubscribes a client from a key and removes the key from the session AVL tree.
/// @param client_id Client ID.
/// @param key Key to unsubscribe from.
/// @return 0 if the client was unsubscribed successfully, -1 otherwise.
int kvs_unsubscribe(int client_id, char *key);


/// Initializes a session AVL tree.
/// @param session_id Session ID.
/// @return 0 if the session AVL tree was initialized successfully, -1 otherwise.
int initialize_session_avl(int session_id);


/// Cleans a session AVL tree.
/// @param session_id Session ID.
void clean_session_avl(int session_id);


/// Adds a key to the session AVL tree.
/// @param session_id Session ID.
/// @param key Key to add.
/// @return 0 if the key was added successfully, -1 otherwise.
int add_key_session_avl(int session_id, const char *key);


/// Removes a key from the session AVL tree.
/// @param session_id Session ID.
/// @param key Key to remove.
/// @return 0 if the key was removed successfully, -1 otherwise.
int remove_key_session_avl(int session_id, const char *key);


/// Waits for a given amount of time.
/// @param delay_ms Delay in milliseconds.
void kvs_wait(unsigned int delay_ms);


/// Do a safe fork with all nodes of the hashTable unlocked.
/// @return child's pid to parent process and 0 to child process
pid_t do_fork();

#endif  // KVS_OPERATIONS_H
