#ifndef AVL_H
#define AVL_H

#include <stdlib.h>
#include <pthread.h>

#include "../common/io.h"
#include "../common/safeFunctions.h"

// Key type, can be a integer (session id) or a string (node's key).
typedef enum KeyType {
    KEY_INT,
    KEY_STRING
} KeyType;


// AVL node structure.
typedef struct AVLNode {
    KeyType key_type;        // Indicates if the key is int or string.
    union {
        struct {
            int int_key;     // Integer key.
            int fd;          // File descriptor (only for int_key).
        };
        char *str_key;       // String key.
    } key;
    struct AVLNode *left;    // Pointer to the left node.
    struct AVLNode *right;   // Pointer to the right node.
    int height;              // Height of the node.
} AVLNode;


// AVL tree structure.
typedef struct AVL {
    AVLNode *root;
    pthread_rwlock_t rwl;
} AVL;


/**
 * Retrieves the key from an AVL node.
 *
 * @param node The AVL node from which to retrieve the key.
 * @return A pointer to the key (int or string), or NULL if the node is NULL.
 */
void* get_key(AVLNode *node);


/**
 * Retrieves the fd from an AVL node.
 *
 * @param node The AVL node from which to retrieve the fd.
 * @return Value of fd from the integer key, or -1 otherwise.
 */
int get_fd(AVLNode *node);


/**
 * Retrieves the left node of an AVL node.
 *
 * @param node The AVL node from which to retrieve the left node.
 * @return A pointer to the left node, or NULL if the node is NULL.
 */
AVLNode* get_left_node(AVLNode *node);


/**
 * Retrieves the right node of an AVL node.
 *
 * @param node The AVL node from which to retrieve the right.
 * @return A pointer to the right node, or NULL if the node is NULL.
 */
AVLNode* get_right_node(AVLNode *node);


/**
 * Retrieves the root node of an AVL tree.
 *
 * @param avl The AVL tree from which to retrieve the root node.
 * @return A pointer to the root node, or NULL if the AVL tree is NULL.
 */
AVLNode* get_root(AVL *avl);


/**
 * Locks the write lock of the AVL tree's read-write lock and checks for errors.
 *
 * @param avl The AVL tree whose read-write lock is to be locked.
 * @return 0 on success, -1 on failure.
 */
int avl_wrlock_secure(AVL *avl);


/**
 * Locks the read lock of the AVL tree's read-write lock and checks for errors.
 *
 * @param avl The AVL tree whose read-write lock is to be locked.
 * @return 0 on success, -1 on failure.
 */
int avl_rdlock_secure(AVL *avl);


/**
 * Unlocks the read-write lock of the AVL tree.
 *
 * @param avl The AVL tree whose read-write lock is to be unlocked.
 */
void avl_unlock_secure(AVL *avl);


/**
 * Checks if the key type of the AVL tree is a string.
 *
 * @param avl The AVL tree to check.
 * @return 1 if the key type is a string, 0 otherwise.
 */
int is_AVL_key_string(AVL *avl);


/**
 * Checks if the key type of the AVL node is a string.
 *
 * @param node The AVL node to check.
 * @return 1 if the key type is a string, 0 otherwise.
 */
int is_node_key_string(AVLNode *node);


/**
 * Checks if the key type of the AVL tree is an integer.
 *
 * @param avl The AVL tree to check.
 * @return 1 if the key type is an integer, 0 otherwise.
 */
int is_AVL_key_int(AVL *avl);


/**
 * Checks if the key type of the AVL node is a integer.
 *
 * @param node The AVL node to check.
 * @return 1 if the key type is a integer, 0 otherwise.
 */
int is_node_key_int(AVLNode *node);


/**
 * Creates a new AVL tree.
 *
 * @return A pointer to the newly created AVL tree, or NULL on failure.
 */
AVL *create_avl();


/**
 * Adds a key-fd pair to the AVL tree.
 *
 * @param avl The AVL tree to which the key-fd pair will be added.
 * @param key The key to be added.
 * @param fd The file descriptor associated with the key.
 *
 * @return 0 if the pair was added successfully, -1 otherwise.
 */
int avl_add(AVL *avl, void* key, int fd);


/**
 * Removes a node from the AVL tree by key.
 *
 * @param avl The AVL tree from which the node will be removed.
 * @param key The key of the node to be removed.
 *
 * @return 0 if the node was removed successfully, -1 otherwise.
 */
int avl_remove(AVL *avl, void* key);


/**
 * Checks if a key exists in the AVL tree.
 *
 * @param avl The AVL tree to search.
 * @param key The key to search for.
 *
 * @return 1 if the key was found, 0 otherwise.
 */
int has_key(AVL *avl, void* key);


/**
 * Sends a message to all fds in the AVL tree.
 *
 * @param avl The AVL tree containing the fds.
 * @param message The message to be sent.
 * @param size The size of the message.
 *
 * @return 0 if the message was sent to all fds successfully, -1 otherwise.
 */
int send_to_all_fds(AVL *avl, const char *message, size_t size);


/**
 * Frees the AVL tree and all its nodes.
 *
 * @param avl The AVL tree to be freed.
 *
 * @return 0 if the AVL tree was freed successfully, -1 otherwise.
 */
int free_avl(AVL *avl);


/**
 * Clean the AVL tree by freeing all its nodes.
 *
 * @param avl The AVL tree to be freed.
 *
 * @return 0 if the AVL tree was freed successfully, -1 otherwise.
 */
int clean_avl(AVL *avl);


/**
 * Applies a function to each node in the AVL tree.
 *
 * @param avl The AVL tree.
 * @param func The function to apply to each node, args are (int, char*).
 * @param int_value First arg of func and needs to be an integer.
 * @return 0 on success, -1 if any error occurs.
 */
int apply_to_all_nodes(AVL *avl, int (*func)(int, char *), int int_value);


#endif // AVL_H
