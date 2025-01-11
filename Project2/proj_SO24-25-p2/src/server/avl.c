#include "avl.h"


/**
 * Creates a new node for the AVL tree.
 *
 * @param key The key to be stored in the node.
 * @param fd The file descriptor associated with the key.
 *
 * @return A pointer to the newly created node, or NULL on failure.
 */
AVLNode *create_node(int key, int fd) {
    AVLNode *node = malloc(sizeof(AVLNode));

    if (!node) return NULL;

    node->key = key;
    node->fd = fd;
    node->left = NULL;
    node->right = NULL;
    node->height = 1; // New node is initially added at leaf
    return node;
}


/**
 * Returns the height of the node.
 *
 * @param node The node whose height is to be returned.
 *
 * @return The height of the node, or 0 if the node is NULL.
 */
int height(AVLNode *node) {
    return (node == NULL) ? 0 : node->height;
}


/**
 * Returns the maximum of two integers.
 *
 * @param a The first integer.
 * @param b The second integer.
 *
 * @return The maximum of the two integers.
 */
int max(int a, int b) {
    return (a > b) ? a : b;
}


/**
 * Right rotates the subtree rooted with y.
 *
 * @param y The root of the subtree to be right rotated.
 *
 * @return The new root of the subtree after rotation.
 */
AVLNode *right_rotate(AVLNode *y) {
    AVLNode *x = y->left;
    AVLNode *T2 = x->right;

    // Perform rotation
    x->right = y;
    y->left = T2;

    // Update heights
    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;   // Return new root
}


/**
 * Left rotates the subtree rooted with y.
 *
 * @param y The root of the subtree to be left rotated.
 *
 * @return The new root of the subtree after rotation.
 */
AVLNode *left_rotate(AVLNode *y) {
    AVLNode *x = y->right;
    AVLNode *T2 = x->left;

    // Perform rotation
    x->left = y;
    y->right = T2;

    // Update heights
    y->height = max(height(y->left), height(y->right)) + 1;
    x->height = max(height(x->left), height(x->right)) + 1;

    return x;   // Return new root
}


/**
 * Get the balance factor of the node.
 *
 * @param node The node whose balance factor is to be calculated.
 *
 * @return The balance factor of the node.
 */
int get_balance(AVLNode *node) {
    return (node == NULL) ? 0 : height(node->left) - height(node->right);
}


/**
 * Balances the AVL tree node.
 *
 * @param node The node to be balanced.
 *
 * @return The new root of the subtree after balancing.
 */
AVLNode *balance_node(AVLNode *node) {
    int balance = get_balance(node);

    // Left Left Case
    if (balance > 1 && get_balance(node->left) >= 0) {
        return right_rotate(node);
    }

    // Left Right Case
    if (balance > 1 && get_balance(node->left) < 0) {
        node->left = left_rotate(node->left);
        return right_rotate(node);
    }

    // Right Right Case
    if (balance < -1 && get_balance(node->right) <= 0) {
        return left_rotate(node);
    }

    // Right Left Case
    if (balance < -1 && get_balance(node->right) > 0) {
        node->right = right_rotate(node->right);
        return left_rotate(node);
    }

    // Tree already balanced
    return node;
}

/**
 * Finds the node with the minimum key value in the subtree.
 *
 * @param node The root of the subtree.
 *
 * @return The node with the minimum key value.
 */
AVLNode *min_value_node(AVLNode *node) {
    AVLNode *current = node;

    while (current->left != NULL) current = current->left;

    return current;
}


/**
 * Inserts a key-fd pair into the AVL tree.
 *
 * @param node The root of the subtree where the key-fd pair will be inserted.
 * @param key The key to be inserted.
 * @param fd The file descriptor associated with the key.
 *
 * @return The new root of the subtree after insertion.
 */
AVLNode *insert_node(AVLNode *node, int key, int fd) {
    int balance;

    if (node == NULL) // No more operations are needed.
        return create_node(key, fd);

    if (key < node->key)
        node->left = insert_node(node->left, key, fd);

    else if (key > node->key)
        node->right = insert_node(node->right, key, fd);

    else
        return node; // Duplicate keys won't result in anything.

    node->height = 1 + max(height(node->left), height(node->right));

    return balance_node(node); // In order for the tree to stay balanced.
}


/**
 * Removes a node from the AVL tree by key.
 *
 * @param root The root of the subtree where the key will be removed.
 * @param key The key of the node to be removed.
 *
 * @return The new root of the subtree after removal.
 */
AVLNode *remove_node(AVLNode *root, int key) {
    if (root == NULL) return root;

    if (key < root->key) {
        root->left = remove_node(root->left, key);
    }
    else if (key > root->key) {
        root->right = remove_node(root->right, key);
    }
    else { // found node to be removed
        if ((root->left == NULL) || (root->right == NULL)) {
            AVLNode *temp = root->left ? root->left : root->right;

            if (temp == NULL) { // Both left and right are null nodes
                temp = root;    // so this node is a leaf.
                root = NULL;
            }
            else *root = *temp; // New root is the left/right node.

            free(temp);
        }
        else {  // In case both left and right nodes exist.
            AVLNode *temp = min_value_node(root->right);
            // Put right min value as root and then removes it previous node.
            root->key = temp->key;
            root->fd = temp->fd;
            root->right = remove_node(root->right, temp->key);
        }
    }

    if (root == NULL) return root; // If the only tree's node was removed.

    root->height = 1 + max(height(root->left), height(root->right));

    return balance_node(root); // In order for the tree to stay balanced.
}


AVL *create_avl() {
    AVL *avl = malloc(sizeof(AVL));

    if (!avl) return NULL;

    avl->root = NULL;
    if (pthread_rwlock_init(&avl->rwl, NULL)){
        free(avl);
        return NULL;
    }

    return avl;
}


int avl_add(AVL *avl, int key, int fd) {
    if (!avl || pthread_rwlock_wrlock_error_check(&avl->rwl, NULL) < 0)
        return -1;

    avl->root = insert_node(avl->root, key, fd);

    pthread_rwlock_unlock(&avl->rwl);
    return 0;
}


int avl_remove(AVL *avl, int key) {
    if (!avl || pthread_rwlock_wrlock_error_check(&avl->rwl, NULL) < 0)
        return -1;

    avl->root = remove_node(avl->root, key);

    pthread_rwlock_unlock(&avl->rwl);
    return 0;
}


int has_fd(AVL *avl, int key, int *fd) {
    AVLNode *current;

    if(pthread_rwlock_rdlock_error_check(&avl->rwl, NULL) < 0) return -1;
    current = avl->root;

    while (current) {
        if (key < current->key) {
            current = current->left;

        } else if (key > current->key) {
            current = current->right;

        } else {
            *fd = current->fd;
            pthread_rwlock_unlock(&avl->rwl);
            return 0; // found
        }
    }

    pthread_rwlock_unlock(&avl->rwl);
    return -1; // not found
}


/**
 * Recursively sends a message to all fds in the AVL tree.
 *
 * @param node The current node in the AVL tree.
 * @param message The message to be sent.
 * @param size The size of the message.
 *
 * @return The number of errors encountered during the send operation.
 */
int send_to_all_fds_recursive(AVLNode *node, const char *message, size_t size) {
    int gotError = 0; // Incremented when a message couldn't be sent to a fd.

    if (node) {
        // send to left sub-tree
        gotError += send_to_all_fds_recursive(node->left, message, size);
        // send to current fd, write returns 1 on success, so we subtract
        gotError += 1 - write_all(node->fd, message, size);
        // send to right sub-tree
        gotError += send_to_all_fds_recursive(node->right, message, size);

        return gotError;
    }
    return 0;
}


int send_to_all_fds(AVL *avl, const char *message, size_t size) {
    int gotError = 0; // Incremented when a message couldn't be sent to a fd.

    if(pthread_rwlock_rdlock_error_check(&avl->rwl, NULL) < 0) return -1;

    gotError = send_to_all_fds_recursive(avl->root, message, size);

    pthread_rwlock_unlock(&avl->rwl);
    return (gotError) ? -1 : 0;
}

/**
 * Frees all nodes in the AVL tree.
 *
 * @param node The root node of the AVL tree to be freed.
 */
void free_avl_node(AVLNode *node) {
    if (node) {
        free_avl_node(node->left);
        free_avl_node(node->right);
        free(node);
    }
}


int free_avl(AVL *avl) {
    int destroy_result;
    if (pthread_rwlock_wrlock_error_check(&avl->rwl, NULL) < 0) return -1;

    free_avl_node(avl->root);
    pthread_rwlock_unlock(&avl->rwl);

    destroy_result = pthread_rwlock_destroy(&avl->rwl);
    free(avl);
    return destroy_result;
}
