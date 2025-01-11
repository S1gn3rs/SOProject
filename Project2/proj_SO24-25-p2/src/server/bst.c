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
    BSTNode **current;

    if (!bst || pthread_rwlock_wrlock_error_check(&bst->rwl, NULL) < 0) return -1;

    current = &bst->root;
    while (*current) {
        if (key < (*current)->key) {
            current = &(*current)->left;

        } else if (key > (*current)->key) {
            current = &(*current)->right;

        } else {
            (*current)->fd = fd; //If fd already exists change file descriptor to the new one.-----------------------------------------------------------------------
            pthread_rwlock_unlock(&bst->rwl);
            return 0;
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
