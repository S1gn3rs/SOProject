#ifndef BST_H
#define BST_H

#include <stdlib.h>
#include <pthread.h>

#include "../common/io.h"
#include "../common/safeFunctions.h"


// Node of the Binary Search Tree (BST)
typedef struct BSTNode {
    int key;
    int fd;
    struct BSTNode *left;   // Pointer to the left child node
    struct BSTNode *right;  // Pointer to the right child node
} BSTNode;


// Binary Search Tree (BST) structure
typedef struct BST {
    BSTNode *root;
    pthread_rwlock_t rwl;
} BST;


/**
 * Creates a new binary search tree (BST).
 *
 * @return A pointer to the newly created BST, or NULL on failure.
 */
BST *create_bst();


/**
 * Adds a key-fd pair to the BST.
 *
 * @param bst The BST to which the key-fd pair will be added.
 * @param key The key to be added.
 * @param fd The file descriptor associated with the key.
 *
 * @return 0 if the pair was added successfully, -1 otherwise.
 */
int bst_add(BST *bst, int key, int fd);


/**
 * Checks if a key exists in the BST and retrieves its associated fd.
 *
 * @param bst The BST to search.
 * @param key The key to search for.
 * @param fd A pointer to store the associated fd if the key is found.
 *
 * @return 0 if the key was found, -1 otherwise.
 */
int has_notif(BST *bst, int key, int *fd);


/**
 * Sends a message to all fds in the BST.
 *
 * @param bst The BST containing the fds.
 * @param message The message to be sent.
 * @param size The size of the message.
 *
 * @return 0 if the message was sent to all fds successfully, -1 otherwise.
 */
int send_to_all_fds(BST *bst, const char *message, size_t size);


/**
 * Frees the BST and all its nodes.
 *
 * @param bst The BST to be freed.
 *
 * @return 0 if the BST was freed successfully, -1 otherwise.
 */
int free_bst(BST *bst);


#endif // BST_H