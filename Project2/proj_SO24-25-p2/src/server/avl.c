#include "avl.h"


void* get_key(AVLNode *node) {
    if (node == NULL) return NULL;

    if (node->key_type == KEY_INT) return &node->key.int_key;

    else if (node->key_type == KEY_STRING) return node->key.str_key;

    return NULL; // Should never reach here
}

int get_fd(AVLNode *node) {
    if (node == NULL || node->key_type != KEY_INT) return -1;

    return node->key.fd;
}


AVLNode* get_left_node(AVLNode *node) {
    if (node == NULL) return NULL;

    return node->left;
}


AVLNode* get_right_node(AVLNode *node) {
    if (node == NULL) return NULL;

    return node->right;
}


AVLNode* get_root(AVL *avl) {
    if (avl == NULL) return NULL;

    return avl->root;
}


int is_AVL_key_string(AVL *avl) {
    if (avl == NULL || avl->root == NULL) return 0;

    return avl->root->key_type == KEY_STRING;
}

int is_node_key_string(AVLNode *node) {
        if (node == NULL) return 0;

    return node->key_type == KEY_STRING;
}


int is_AVL_key_int(AVL *avl) {
    if (avl == NULL || avl->root == NULL) return 0;

    return avl->root->key_type == KEY_INT;
}

int is_node_key_int(AVLNode *node) {
        if (node == NULL) return 0;

    return node->key_type == KEY_INT;
}


int avl_wrlock_secure(AVL *avl) {
    if (avl == NULL) return -1;

    return pthread_rwlock_wrlock_error_check(&avl->rwl, NULL);
}


int avl_rdlock_secure(AVL *avl) {
    if (avl == NULL) return -1;

    return pthread_rwlock_rdlock_error_check(&avl->rwl, NULL);
}


void avl_unlock_secure(AVL *avl) {
    pthread_rwlock_unlock(&avl->rwl);
}


/**
 * Destroys the read-write lock of the AVL tree.
 *
 * @param avl The AVL tree whose read-write lock is to be destroyed.
 * @return 0 on success, an error number on failure.
 */
int avl_destroy_secure(AVL *avl){
    return pthread_rwlock_destroy(&avl->rwl);
}


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
 * @return The balance factor of the node.
 */
int get_balance(AVLNode *node) {
    return (node == NULL) ? 0 : height(node->left) - height(node->right);
}


/**
 * Balances the AVL tree node.
 *
 * @param node The node to be balanced.
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
 * @return The new root of the subtree after insertion.
 */
AVLNode *insert_node(AVLNode *root, void* key, int fd) {
    int comparation = 0;

    if (root == NULL){ // No more operations are needed.
        if (fd == -1) return create_node(KEY_STRING, key, fd);
        return create_node(KEY_INT, key, fd);
    }
    comparation = compare_keys(root->key_type, key, get_key(root));
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

    comparation = compare_keys(root->key_type, key, get_key(root));

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
                if (is_node_key_string(root) && get_key(root))
                        free(get_key(root));

                *root = *temp; // New root is the left/right node.

                if (is_node_key_string(root) && get_key(temp))
                    root->key.str_key = strdup_error_check(get_key(temp));
            }

            if (is_node_key_string(temp) && get_key(temp)) {
                free(get_key(temp)); // Free the string key in the temp node
            }
            free(temp);
        }
        else {  // In case both left and right nodes exist.
            AVLNode *temp = min_value_node(root->right);


            // Free the current node's string key, if applicable
            if (is_node_key_string(root) && get_key(root)) {
                free(get_key(root));
            }

            // Copy the key and fd from the temp node to the current node
            if (is_node_key_string(temp) && get_key(temp)) {
                root->key.str_key = strdup_error_check(get_key(temp));
            }
            else {
                root->key.int_key = *(int*) get_key(temp); // Copy integer key
                root->key.fd =  get_fd(temp);           // Copy file descriptor
            }

            // Remove the minimum node from the right subtree
            root->right = remove_node(root->right, get_key(temp));
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


int avl_add(AVL *avl, void* key, int fd) {
    if (avl_wrlock_secure(avl)) return -1;

    avl->root = insert_node(avl->root, key, fd);

    avl_unlock_secure(avl);

    return 0;
}


int avl_remove(AVL *avl, void* key) {
    if (avl_wrlock_secure(avl)) return -1;

    avl->root = remove_node(avl->root, key);

    avl_unlock_secure(avl);
    return 0;
}


int has_key(AVL *avl, void *key) {
    AVLNode *current;
    int comparation;

    if (avl_rdlock_secure(avl)){
        return 0;
    }

    current = avl->root;

    while (current) {

        comparation = compare_keys(current->key_type, key, get_key(current));
        if (comparation < 0) {
            current = get_left_node(current);
        } else if (comparation > 0) {
            current = get_right_node(current);
        } else {
            avl_unlock_secure(avl);
            return 1; // Key found
        }
    }

    avl_unlock_secure(avl);
    return 0; // Key not found
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
        gotError += send_to_all_fds_recursive(get_left_node(node), message,\
            size);
        // send to current fd, write returns 1 on success, so we subtract
        gotError += 1 - write_all(get_fd(node), message, size);
        // send to right sub-tree
        gotError += send_to_all_fds_recursive(get_right_node(node), message,\
            size);

        return gotError;
    }
    return 0;
}


int send_to_all_fds(AVL *avl, const char *message, size_t size) {
    int gotError = 0; // Incremented when a message couldn't be sent to a fd.
    if(avl_rdlock_secure(avl)) return -1;
    if (get_root(avl) == NULL){
        avl_unlock_secure(avl);
        return 0;
    }

    if (!is_AVL_key_int(avl)){
        avl_unlock_secure(avl);
        return -1;
    }

    gotError = send_to_all_fds_recursive(get_root(avl), message, size);

    avl_unlock_secure(avl);

    return (gotError) ? -1 : 0;
}


/**
 * Frees all nodes in the AVL tree.
 *
 * @param node The root node of the AVL tree to be freed.
 */
void free_avl_node(AVLNode *node) {
    if (node) {
        free_avl_node(get_left_node(node));
        free_avl_node(get_right_node(node));

        if (is_node_key_string(node))
            free(node->key.str_key);

        free(node);
    }
}


int free_avl(AVL *avl) {
    int destroy_result;

    if (avl_wrlock_secure(avl)) return -1;

    free_avl_node(get_root(avl));
    avl_unlock_secure(avl);

    destroy_result = avl_destroy_secure(avl);
    free(avl);
    return destroy_result;
}


int clean_avl(AVL *avl){
    if (avl_wrlock_secure(avl)) return -1;

    free_avl_node(get_root(avl));
    avl->root = NULL;

    avl_unlock_secure(avl);

    return 0;
}


/**
 * Recursively applies a function to each node in the AVL tree.
 *
 * @param node The current node in the AVL tree.
 * @param func The function to apply to each node, args are (int, char*).
 * @param int_value First arg of func and needs to be an integer.
 */
int apply_to_all_nodes_recursive(AVLNode *node, int (*func)(int, char *),\
    int int_value) {

    int error = 0;

    if (node) {

        // Apply function to left sub-tree
        apply_to_all_nodes_recursive(get_left_node(node), func, int_value);
        // Apply function to current node
        error += func(int_value, get_key(node));
        // Apply function to right sub-tree
        apply_to_all_nodes_recursive(get_right_node(node), func, int_value);
    }
    return error;
}


int apply_to_all_nodes(AVL *avl, int (*func)(int, char *), int int_value) {
    int error = 0;

    if (!avl){
        return -1;
    }
    if (!get_root(avl)){
        return 0;
    }
    if (avl_rdlock_secure(avl)){
        return -1;
    }

    if (!is_AVL_key_string(avl)){
        avl_unlock_secure(avl);
        return -1;
    }

    error = apply_to_all_nodes_recursive(get_root(avl), func, int_value);

    avl_unlock_secure(avl);

    return error ? -1 : 0;
}
