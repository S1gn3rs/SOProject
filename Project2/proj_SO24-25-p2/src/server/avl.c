#include "avl.h"


/**
 * Creates a new AVL node.
 *
 * @param key_type The type of the key (KEY_INT or KEY_STRING).
 * @param key The key value (int or string).
 * @param fd The file descriptor (only for (int) key, use -1 if not applicable).
 * @return A pointer to the newly created AVL node.
 */
AVLNode *create_node(KeyType key_type, void* key, int fd) {
    AVLNode *node = malloc(sizeof(AVLNode));

    if (!node) return NULL;

    node->key_type = key_type;
    if (key_type == KEY_INT && fd != -1) {
        node->key.int_key = *(int *)key;
        node->key.fd = fd;
    }
    else if(key_type == KEY_STRING){
        node->key.str_key = strdup((char *)key);
        if (!node->key.str_key){ // Handle memory allocation failure
            free(node);
            return NULL;
        }
    }
    else {
        free(node);
        return NULL;
    }
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
 * Compares two keys of type KeyType.
 *
 * @param key_type The type of the keys (KEY_INT or KEY_STRING).
 * @param key1 The first key to compare.
 * @param key2 The second key to compare.
 * @return A negative value if key1 is less than key2,
 *         0 if key1 is equal to key2,
 *         a positive value if key1 is greater than key2.
 */
int compare_keys(KeyType key_type, void *key1, void *key2) {
    if (key_type == KEY_INT) {
        return (*(int *)key1 - *(int *)key2);
    }
    else if (key_type == KEY_STRING) {
        return strcmp((char *)key1, (char *)key2);
    }
    return 0; // Should never reach here
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
 * @param root The root of the subtree where the key-fd pair will be inserted.
 * @param key The key to be inserted.
 * @param fd The file descriptor (only for (int) key, use -1 if not applicable).
 *
 * @return The new root of the subtree after insertion.
 */
AVLNode *insert_node(AVLNode *root, void* key, int fd) {
    int balance, comparation = 0;

    if (root == NULL) // No more operations are needed.
        return create_node(root->key_type, key, fd);

    if (root->key_type == KEY_INT)
        comparation = compare_keys(root->key_type, key, &root->key.int_key);

    else if (root->key_type == KEY_STRING)
        comparation = compare_keys(root->key_type, key, root->key.str_key);

    if (comparation < 0)
        root->left = insert_node(root->left, key, fd);

    else if (comparation > 0)
        root->right = insert_node(root->right, key, fd);

    else
        return root; // Duplicate keys won't result in anything.

    root->height = 1 + max(height(root->left), height(root->right));

    return balance_node(root); // In order for the tree to stay balanced.
}


/**
 * Removes a node from the AVL tree by key.
 *
 * @param root The root of the subtree where the key will be removed.
 * @param key The key of the node to be removed.
 *
 * @return The new root of the subtree after removal.
 */
AVLNode *remove_node(AVLNode *root, void* key) {
    int comparation = 0;

    if (root == NULL) return root;

    if (root->key_type == KEY_INT)
        comparation = compare_keys(root->key_type, key, &root->key.int_key);
    else if (root->key_type == KEY_STRING)
        comparation = compare_keys(root->key_type, key, root->key.str_key);

    if (comparation < 0)
        root->left = remove_node(root->left, key);
    else if (comparation > 0)
        root->right = remove_node(root->right, key);
    else { // found node to be removed
        if ((root->left == NULL) || (root->right == NULL)) {
            AVLNode *temp = root->left ? root->left : root->right;

            if (temp == NULL) { // Both left and right are null nodes
                temp = root;    // so this node is a leaf.
                root = NULL;
            }
            else{ // 1 (left or right) isn't null.
                if (root->key_type == KEY_STRING && root->key.str_key)
                        free(root->key.str_key);

                *root = *temp; // New root is the left/right node.
                if (root->key_type == KEY_STRING && temp->key.str_key)
                    root->key.str_key = strdup_error_check(temp->key.str_key);
            }

            if (temp->key_type == KEY_STRING && temp->key.str_key) {
                free(temp->key.str_key); // Free the string key in the temp node
            }
            free(temp);
        }
        else {  // In case both left and right nodes exist.
            AVLNode *temp = min_value_node(root->right);


            // Free the current node's string key, if applicable
            if (root->key_type == KEY_STRING && root->key.str_key) {
                free(root->key.str_key);
            }

            // Copy the key and fd from the temp node to the current node
            if (temp->key_type == KEY_STRING && temp->key.str_key) {
                root->key.str_key = strdup_error_check(temp->key.str_key);
            }
            else {
                root->key.int_key = temp->key.int_key; // Copy integer key
                root->key.fd = temp->key.fd;           // Copy file descriptor
            }

            // Remove the minimum node from the right subtree
            if (temp->key_type == KEY_STRING)
                root->right = remove_node(root->right, temp->key.str_key);
            else if (temp->key_type == KEY_INT)
                root->right = remove_node(root->right, temp->key.int_key);
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


int has_fd(AVL *avl, void* key, int *fd) {
    AVLNode *current;
    int comparation;

    if(pthread_rwlock_rdlock_error_check(&avl->rwl, NULL) < 0) return -1;
    current = avl->root;

    if (avl->root->key_type != KEY_INT){
        pthread_rwlock_unlock(&avl->rwl);
        return -1;
    }

    while (current) {
        comparation = compare_keys(KEY_INT, key, &current->key.int_key);
        if (comparation < 0) {
            current = current->left;

        } else if (comparation > 0) {
            current = current->right;

        } else {
            *fd = current->key.fd;
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
        gotError += 1 - write_all(node->key.fd, message, size);
        // send to right sub-tree
        gotError += send_to_all_fds_recursive(node->right, message, size);

        return gotError;
    }
    return 0;
}


int send_to_all_fds(AVL *avl, const char *message, size_t size) {
    int gotError = 0; // Incremented when a message couldn't be sent to a fd.

    if(pthread_rwlock_rdlock_error_check(&avl->rwl, NULL) < 0) return -1;
    if (avl->root->key_type != KEY_INT) return -1;

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

        if (node->key_type == KEY_STRING)
            free(node->key.str_key);

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
