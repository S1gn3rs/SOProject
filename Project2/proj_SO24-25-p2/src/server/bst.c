#include "bst.h"


/**
 * Creates a new node for the binary search tree (BST).
 *
 * @param key The key to be stored in the node.
 * @param notif_fd The file descriptor associated with the key.
 *
 * @return A pointer to the newly created node, or NULL on failure.
 */
BSTNode *create_node(int key, int fd) {
    BSTNode *node = malloc(sizeof(BSTNode));

    if (!node) return NULL;

    node->key = key;
    node->fd = fd;
    node->left = NULL;
    node->right = NULL;
    return node;
}


BST *create_bst() {
    BST *bst = malloc(sizeof(BST));

    if (!bst) return NULL;

    bst->root = NULL;
    if (pthread_rwlock_init(&bst->rwl, NULL)){
        free(bst);
        return NULL;
    }

    return bst;
}


int bst_add(BST *bst, int key, int fd) {
    BSTNode **current; // Double pointer to be able to traverse and modify the tree.

    if (!bst || pthread_rwlock_wrlock_error_check(&bst->rwl, NULL) < 0) return -1;

    current = &bst->root; // Start at the root of the tree
    while (*current) {  // Traverse the tree
        if (key < (*current)->key) { // Move to the left node
            current = &(*current)->left;

        } else if (key > (*current)->key) { // Move to the right node
            current = &(*current)->right;

        } else {
            pthread_rwlock_unlock(&bst->rwl);
            return -1; // That key already exists inside of the bst
        }
    }
    *current = create_node(key, fd);

    if (!*current) {
        pthread_rwlock_unlock(&bst->rwl);
        return -1; // Could'n allocate memory.
    }

    pthread_rwlock_unlock(&bst->rwl);
    return 0;
}


int has_notif(BST *bst, int key, int *fd) {
    BSTNode *current;

    if(pthread_rwlock_rdlock_error_check(&bst->rwl, NULL) < 0) return -1;
    current = bst->root;

    while (current) {
        if (key < current->key) {
            current = current->left;

        } else if (key > current->key) {
            current = current->right;

        } else {
            *fd = current->fd;
            pthread_rwlock_unlock(&bst->rwl);
            return 0; // found
        }
    }

    pthread_rwlock_unlock(&bst->rwl);
    return -1; // not found
}


/**
 * Removes a node from the BST by key.
 *
 * @param bst The BST from which the node will be removed.
 * @param key The key of the node to be removed.
 *
 * @return 0 if the node was removed successfully, -1 otherwise.
 */
int bst_remove(BST *bst, int key) {
    BSTNode **current, *node_to_remove, *replacement_node;

    if (!bst || pthread_rwlock_wrlock_error_check(&bst->rwl, NULL) < 0) return -1;

    current = &bst->root;
    while (*current) {
        if (key < (*current)->key) {
            current = &(*current)->left;

        } else if (key > (*current)->key) {
            current = &(*current)->right;

        } else {
            node_to_remove = *current;  // Node to be removed found

            if (!node_to_remove->left) { // Has no left node, replace with right node
                *current = node_to_remove->right;
            } else if (!node_to_remove->right) {
                *current = node_to_remove->left; // Has no right node, replace with left node

            } else {    // Has both nodes, find the in-order successor
                BSTNode **replacement_parent;
                replacement_node = node_to_remove->right;
                replacement_parent = &node_to_remove->right;

                while (replacement_node->left) {
                    replacement_parent = &replacement_node->left;
                    replacement_node = replacement_node->left;
                }
                // Replace node_to_remove with replacement_node
                *replacement_parent = replacement_node->right;
                replacement_node->left = node_to_remove->left;
                replacement_node->right = node_to_remove->right;
                *current = replacement_node;
            }

            free(node_to_remove);
            pthread_rwlock_unlock(&bst->rwl);
            return 0; // Node removed successfully
        }
    }

    pthread_rwlock_unlock(&bst->rwl);
    return -1; // Node not found
}


int send_to_all_fds(BST *bst, const char *message, size_t size) {
    int gotError = 0;

    if(pthread_rwlock_rdlock_error_check(&bst->rwl, NULL) < 0) return -1;

    gotError = send_to_all_fds_recursive(bst->root, message, size);

    pthread_rwlock_unlock(&bst->rwl);
    return (gotError) ? -1 : 0;
}



/**
 * Recursively sends a message to all fds in the BST.
 *
 * @param node The current node in the BST.
 * @param message The message to be sent.
 * @param size The size of the message.
 *
 * @return The number of errors encountered during the send operation.
 */
int send_to_all_fds_recursive(BSTNode *node, const char *message, size_t size) {
    int gotError = 0;
    if (node) {
        gotError += send_to_all_fds_recursive(node->left, message, size); // send to left sub-tree
        gotError += 1 - write_all(node->fd, message, size); // send to current fd, como retorna 1 no sucesso, é feita a subtração
        gotError += send_to_all_fds_recursive(node->right, message, size); // send to right sub-tree
        return gotError;
    }
    return 0;
}


/**
 * Frees all nodes in the BST.
 *
 * @param node The root node of the BST to be freed.
 */
void free_bst_node(BSTNode *node) {
    if (node) {
        free_bst_node(node->left);
        free_bst_node(node->right);
        free(node);
    }
}


int free_bst(BST *bst) {
    if (pthread_rwlock_wrlock_error_check(&bst->rwl, NULL) < 0) return -1;
    free_bst_node(bst->root);
    pthread_rwlock_unlock(&bst->rwl);
    int destroy_result = pthread_rwlock_destroy(&bst->rwl);
    free(bst);
    return destroy_result;
}
