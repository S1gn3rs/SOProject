#ifndef AVL_H
#define AVL_H

#include <stdlib.h>
#include <pthread.h>

#include "../common/io.h"
#include "../common/safeFunctions.h"


// Node of the AVL tree.
typedef struct AVLNode {
    int key;
    int fd;
    struct AVLNode *left;   // Pointer to the left child node
    struct AVLNode *right;  // Pointer to the right child node
    int height;             // Height of the node
} AVLNode;


// AVL tree structure
typedef struct AVL {
    AVLNode *root;
    pthread_rwlock_t rwl;
} AVL;


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
int avl_add(AVL *avl, int key, int fd);


/**
 * Removes a node from the AVL tree by key.
 *
 * @param avl The AVL tree from which the node will be removed.
 * @param key The key of the node to be removed.
 *
 * @return 0 if the node was removed successfully, -1 otherwise.
 */
int avl_remove(AVL *avl, int key);


/**
 * Checks if a key exists in the AVL tree and retrieves its associated fd.
 *
 * @param avl The AVL tree to search.
 * @param key The key to search for.
 * @param fd A pointer to store the associated fd if the key is found.
 *
 * @return 0 if the key was found, -1 otherwise.
 */
int has_fd(AVL *avl, int key, int *fd);


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


#endif // AVL_H
