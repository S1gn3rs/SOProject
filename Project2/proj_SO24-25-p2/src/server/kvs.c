#include "kvs.h"

int hash(const char *key) {
    int first_letter = tolower(key[0]);
    if (first_letter >= 'a' && first_letter <= 'z') {
        return first_letter - 'a';
    } else if (first_letter >= '0' && first_letter <= '9') {
        return first_letter - '0';
    }
    return -1; // Invalid index for non-alphabetic or number strings
}


/**
 * Function to create a new IndexList.
 *
 * @return a pointer to the new IndexList or NULL on failure.
 */
IndexList* create_IndexList(){
  IndexList *index_list = malloc(sizeof(IndexList));
  if (!index_list) return NULL;
  if (pthread_rwlock_init(&index_list->rwl, NULL)){
    free(index_list);
    return NULL;
  }
  index_list->head = NULL;
  return index_list;
}


HashTable* create_hash_table() {
  HashTable *ht = malloc(sizeof(HashTable));
  if (!ht) return NULL;
  if (pthread_rwlock_init(&ht->rwl, NULL)){
    free(ht);
    return NULL;
  }
  // Initialize the index lists
  for (int i = 0; i < TABLE_SIZE; i++){
    ht->table[i] = create_IndexList();
  }
  return ht;
}



int write_pair(HashTable *ht, const char *key, const char *value,\
    const char *notif_message) {

    int index = hash(key);
    KeyNode *key_node, *new_key_node;
    IndexList *index_list = ht->table[index];

    key_node = index_list->head;
    // Search for the key node

    while (key_node != NULL) {
        // If the key is found, update the value

        if (strcmp(key_node->key, key) == 0) {
            free(key_node->value);
            key_node->value = strdup_error_check(value);

            if (key_node->value == NULL) return -1;
            send_to_all_fds(key_node->avl_notif_fds, notif_message,\
                2 * MAX_STRING_SIZE + 2);

            return 0;
        }
        key_node = key_node->next; // Move to the next node
    }
    // Key not found; create a new key node
    new_key_node = malloc(sizeof(KeyNode));
    if (!new_key_node) return -1;
    // Copy the key to the new key node
    new_key_node->key = strdup_error_check(key);
    if (new_key_node->key == NULL){
        free(new_key_node);
        return -1;
    }
    // Copy the value to the new key node
    new_key_node->value = strdup_error_check(value);
    if (new_key_node->value == NULL) {
        free(new_key_node->key);
        free(new_key_node);
        return -1;
    }

    new_key_node->avl_notif_fds = create_avl();

    if(new_key_node->avl_notif_fds == NULL) {
        free(new_key_node->value);
        free(new_key_node->key);
        free(new_key_node);
        return -1;
    }
    // Insert the new key node at the beginning of the list
    new_key_node->next = (index_list->head != NULL)? index_list->head : NULL;
    index_list->head = new_key_node;

    return 0;
}


char* read_pair(HashTable *ht, const char *key) {
    int index = hash(key);
    char* value;
    IndexList *index_list = ht->table[index];
    KeyNode *key_node = index_list->head;

    // Search for the key node
    while (key_node != NULL) {
        // If the key is found, return a copy of the value
        if (strcmp(key_node->key, key) == 0) {
            value = strdup_error_check(key_node->value);
            return value; // Return copy of the value if found
        }
        key_node = key_node->next; // Move to the next node
    }
    return NULL; // Key not found
}


/**
 * Recursively removes subscriptions from avl_sessions based on the keys in
 * avl_kvs_node.
 *
 * @param node The current node in the avl_kvs_node tree.
 * @param avl_sessions An array of AVL trees representing session subscriptions.
 * @param key String of kvs node's key to remove from sessions' avl.
 *
 * @return 0 on success, -1 if any error occurs.
 */
int remove_node_subscriptions_recursive(AVLNode *root,\
    AVLSessions *avl_sessions, const char* key){

    int result = 0;
    int session_id;

    if (!root) return 0;

    // Remove subscriptions from the left subtree
    result += remove_node_subscriptions_recursive(get_left_node(root),\
        avl_sessions,key);

    session_id = *(int*) get_key(root);

    // Remove subscriptions from current subtree
    if (remove_key_session_avl(session_id, key) < 0) result = -1;
    else dec_num_subs(session_id); // only decrement if before didn't get error.

    // Remove subscriptions from the right subtree
    result += remove_node_subscriptions_recursive(get_left_node(root),\
        avl_sessions,key);

    return result ? -1 : 0;
}


/**
 * Removes subscriptions from avl_sessions based on the keys in avl_kvs_node.
 *
 * @param avl_kvs_node AVL tree containing the keys to remove subscriptions for.
 * @param avl_sessions An array of AVL trees representing session subscriptions.
 * @param key String of kvs node's key to remove from sessions' avl.
 *
 * @return 0 on success, -1 if any error occurs.
 */
int remove_node_subscriptions(AVL *avl_kvs_node, AVLSessions *avl_sessions,\
    const char* key){

    AVLNode *node;
    int result;

    if (avl_wrlock_secure(avl_kvs_node) < 0)
        return -1;

    if ((node = get_root(avl_kvs_node)) == NULL){
        avl_unlock_secure(avl_kvs_node);
        return 0;
    }

    if (!is_AVL_key_int(avl_kvs_node)){
        avl_unlock_secure(avl_kvs_node);
        return -1;
    }

    result = remove_node_subscriptions_recursive(node, avl_sessions, key);

    avl_unlock_secure(avl_kvs_node);
    return result ? -1 : 0;
}


int delete_pair(HashTable *ht, AVLSessions *avl_sessions, const char *key,\
    const char *notif_message) {

    int index = hash(key);
    IndexList *index_list;
    KeyNode *key_node, *prevNode = NULL;
    index_list = ht->table[index];
    key_node = index_list->head;

    // Search for the key node
    while (key_node != NULL) {
        // Key found; delete this node
        if (strcmp(key_node->key, key) == 0) {
            // Node to delete is the first node in the list
            if (prevNode == NULL) {
                // Update the table to point to the next node
                index_list->head = key_node->next;
            } else {// Node to delete is not the first; bypass it
                // Link the previous node to the next node
                prevNode->next = key_node->next;
            }
            send_to_all_fds(key_node->avl_notif_fds, notif_message,\
                2 * MAX_STRING_SIZE + 2);

            remove_node_subscriptions(key_node->avl_notif_fds, avl_sessions,\
                key);

            free_avl(key_node->avl_notif_fds);

            free(key_node->key);
            free(key_node->value);
            free(key_node);

            return 0;
        }
        prevNode = key_node;        // Move prevNode to current node
        key_node = key_node->next;  // Move to the next node
    }
    return -1;
}


int subscribe_pair(HashTable *ht, char key[MAX_STRING_SIZE + 1],\
    int session_id, int notif_fd){

    int index = hash(key);
    IndexList *index_list = ht->table[index];
    KeyNode *key_node = index_list->head;
    // Search for the key node
    while (key_node != NULL) {
        if (strcmp(key_node->key, key) == 0) { // Key is found
            avl_add(key_node->avl_notif_fds, &session_id, notif_fd);
            return 0;
        }
        key_node = key_node->next; // Move to the next node
    }
    return -1;
}


int unsubscribe_pair(HashTable *ht, char key[MAX_STRING_SIZE + 1],\
    int session_id){

    int index = hash(key);
    IndexList *index_list = ht->table[index];
    KeyNode *key_node = index_list->head;
    // Search for the key node
    while (key_node != NULL) {
        if (strcmp(key_node->key, key) == 0) { // Key is found
            avl_remove(key_node->avl_notif_fds, &session_id);
            return 0;
        }
        key_node = key_node->next; // Move to the next node
    }
    return -1;
}


void free_table(HashTable *ht) {
    // Iterate over the hash table
    for (int i = 0; i < TABLE_SIZE; i++) {
        IndexList *index_list = ht->table[i];
        // If the index list is NULL, continue to the next index
        if (index_list == NULL) continue;
        KeyNode *key_node = index_list->head;
        // Iterate over the key nodes
        while (key_node != NULL) {
            KeyNode *temp = key_node;
            key_node = key_node->next;
            free(temp->avl_notif_fds);
            free(temp->key);
            free(temp->value);
            free(temp);
        }
        pthread_rwlock_destroy(&index_list->rwl);
        free(index_list);
    }
    pthread_rwlock_destroy(&ht->rwl);
    free(ht);
}
