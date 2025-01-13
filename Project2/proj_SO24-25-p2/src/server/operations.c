#include "operations.h"

/// AVL tree session to store a total of MAX_SESSIONS sessions.
AVLSessions *avl_sessions = NULL;

/// Hash table to store the key value pairs.
static struct HashTable *kvs_table = NULL;


AVL* get_avl_client(int session_id){
  return avl_sessions->avl_clients_node[session_id];
}


void set_avl_client(int session_id, AVL *new_avl){
  avl_sessions->avl_clients_node[session_id] = new_avl;
}


void set_client_info(int session_id, ClientData *new_client_data){
  avl_sessions->clients_data[session_id] = new_client_data;
}


ClientData* get_client_info(int session_id){
  return avl_sessions->clients_data[session_id];
}

pthread_mutex_t* get_mutex(int sessions_id){
  return &avl_sessions->session_mutex[sessions_id];
}


int get_avl_num_subs(int session_id){
  return avl_sessions->num_subs[session_id];
}


void reset_num_subs(int session_id) {avl_sessions->num_subs[session_id] = 0;}


void inc_num_subs(int session_id) {avl_sessions->num_subs[session_id]++;}


void dec_num_subs(int session_id) {
  // Delete is the only function outside that influences amount of subs and
  // a mutex is needed.
  pthread_mutex_lock(&avl_sessions->session_mutex[session_id]);
  avl_sessions->num_subs[session_id]--;
  pthread_mutex_unlock(&avl_sessions->session_mutex[session_id]);
}


int initialize_session_avl(int session_id){
  int hasError;

  set_avl_client(session_id, create_avl());
  if (get_avl_client(session_id) == NULL) return -1;

  reset_num_subs(session_id); // Puts counter of subscriptions to 0.

  hasError = pthread_mutex_init(get_mutex(session_id), NULL);
  if (hasError){
    free_avl(get_avl_client(session_id));
    set_avl_client(session_id, NULL);
    return -1;
  }
  return 0;
}


/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}


void clean_session_avl(int session_id){
  ClientData *data;
  AVL *avl_subscr = get_avl_client(session_id);

  if (avl_subscr != NULL) clean_avl(avl_subscr);

  data = get_client_info(session_id);
  if (data != NULL){
    close(data->notif_pipe_fd);
    close(data->req_pipe_fd);
    close(data->resp_pipe_fd);
    free(data);
  }

  reset_num_subs(session_id);
}


int add_key_session_avl(int session_id, const char *key){
  if (avl_add(get_avl_client(session_id), (void *) key, -1) != 0) return -1;

  return 0;
}

int remove_key_session_avl(int session_id, const char *key){
  if (avl_remove(get_avl_client(session_id), (void *) key) != 0){
    return -1;
  }
  return 0;
}

int kvs_init() {
  if (kvs_table != NULL) {
    fprintf(stderr, "KVS state has already been initialized\n");
    return -1;
  }
  kvs_table = create_hash_table();
  return kvs_table == NULL;
}


int kvs_terminate() {
  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return -1;
  }
  free_table(kvs_table);
  return 0;
}


int avl_sessions_init(){
  if (avl_sessions != NULL) {
    fprintf(stderr, "AVL sessions have already been initialized\n");
    return -1;
  }

  avl_sessions = malloc(sizeof(AVLSessions));
  for (int session = 0; session < MAX_SESSION_COUNT; session++){
    avl_sessions->clients_data[session] = NULL;
    avl_sessions->avl_clients_node[session] = NULL;
    avl_sessions->num_subs[session] = 0;
  }
  return avl_sessions == NULL;
}


int avl_sessions_terminate() {
  if (avl_sessions == NULL) {
    fprintf(stderr, "AVL sessions state must be initialized\n");
    return -1;
  }
  for (int session_id = 0; session_id < MAX_SESSION_COUNT; session_id++){

    clean_session_avl(session_id); // Remove subscriptions
    // Free tree of subscriptions (similar to a free)
    free_avl(get_avl_client(session_id));

    pthread_mutex_destroy(get_mutex(session_id));

  }
  free(avl_sessions);
  return 0;
}


int avl_clean_sessions() {
  if (avl_sessions == NULL) {
    fprintf(stderr, "AVL sessions state must be initialized\n");
    return -1;
  }
  for (int session_id = 0; session_id < MAX_SESSION_COUNT; session_id++){
    clean_session_avl(session_id);
  }
  return 0;
}


/// Activate kvs hash table's lock to write and checks if it gets any error, if
/// an error ocurred reports it and won't execute the lock.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_wrlock(){
  return pthread_rwlock_wrlock_error_check(&kvs_table->rwl, NULL);
}


/// Activate kvs hash table's lock to read and checks if it gets any error, if
/// an error ocurred reports it and won't execute the lock.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_rdlock(){
  return pthread_rwlock_rdlock_error_check(&kvs_table->rwl, NULL);
}


/// Desactivate kvs hash table's lock (unlock) and checks if it gets any error,
/// if an error ocurred reports it and won't execute the unlock.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_unlock(){
  return pthread_rwlock_unlock(&kvs_table->rwl);
}


/// Activate lock to write of hash table's index list and checks if it gets any
/// error, if an error ocurred reports it and won't execute the lock.
/// @param index To get the corresponding list of that index in the hash table.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_list_wrlock(size_t index){
  return pthread_rwlock_wrlock_error_check(&kvs_table->table[index]->rwl,\
    &kvs_table->rwl);
}


/// Activate lock to read of hash table's index list and checks if it gets any
/// error, if an error ocurred reports it and won't execute the lock.
/// @param index To get the corresponding list of that index in the hash table.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_list_rdlock(size_t index){
  return pthread_rwlock_rdlock_error_check(&kvs_table->table[index]->rwl,\
    &kvs_table->rwl);
}


/// Desactivate lock (unlock) of hash table's index list and checks if it gets
/// any error, if an error ocurred reports it and won't execute the unlock.
/// @param index To get the corresponding list of that index in the hash table.
/// @return 0 if executes successfully and 1 if it gets an error.
int hash_table_list_unlock(size_t index){
  return pthread_rwlock_unlock(&kvs_table->table[index]->rwl);
}


/// Tries to write the content in buffer to fd received and checks errors.
/// @param fd File descriptor to write the output.
/// @param buffer Buffer with the string to write in the fd.
/// @return 0 if the write was successful, 1 otherwise.
int write_error_check(int fd, char *buffer) {
  ssize_t total_written = 0;
  size_t done = 0, buff_len = strlen(buffer);

  // Ensure we write the entire buffer to the file descriptor
  while ((total_written = write(fd, buffer+done, buff_len)) > 0) {
    done += (size_t)total_written;
    buff_len = buff_len - (size_t)total_written;
  }

  // Check if the write was successful
  if (total_written == -1) {
    const char *errorMsg = "Failed to write to .out file\n";
    buff_len = strlen(errorMsg);
    done = 0;
    buff_len = buff_len;
    // Ensure we write the entire error message to stderr
    while ((total_written = write(STDERR_FILENO, errorMsg+done, buff_len)) > 0){
      done += (size_t)total_written;
      buff_len = buff_len - (size_t)total_written;
    }
    return -1;
  }
  return 0;
}


/// A variation of insertion sort that sorts an index list based on it's keys.
/// @param indexs List of indexs to be sorted.
/// @param num_pairs Amount of indexs that need to be sorted by their key.
/// @param keys Correspondent key for each index.
void insertion_sort(size_t *indexs, size_t num_pairs,\
  char keys[][MAX_STRING_SIZE]) {

  // Iterate over the indexs
  for (size_t i = 1; i < num_pairs; i++) {
    size_t current = indexs[i];
    size_t j = i;

    // Find the correct position for the current index
    for(; j > 0 && strcasecmp(keys[indexs[j-1]], keys[current]) > 0; j--)
      indexs[j] = indexs[j - 1];

    // Insert the current index in the correct position
    indexs[j] = current;
  }
}


int kvs_write(size_t num_pairs, char keys[][MAX_STRING_SIZE], \
  char values[][MAX_STRING_SIZE]) {

  int is_locked[26] = {0};  // To indicate which indices are lock
  int error = 0;
  size_t *indexs;           // index of each par

  char notif_message[MAX_STRING_SIZE * 2 + 2];
  char *aux_message;

  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return -1;
  }

  indexs = malloc(num_pairs * sizeof(size_t));
  if (indexs == NULL) {
    fprintf(stderr, "Failed to allocate memory for indexs\n");
    return -1;
  }

  for (size_t i = 0; i < num_pairs; i++) indexs[i] = i;

  if (hash_table_rdlock()){
    free(indexs);
    return -1;
  }


  insertion_sort(indexs, num_pairs, keys); // Sort the indexs based on the keys
  // Iterate over the pairs
  for (size_t ind = 0; ind < num_pairs; ind++) {
    size_t indexNodes = indexs[ind]; // index of the node to write
    size_t indexList = (size_t) hash(keys[indexNodes]);

    if(ind < num_pairs - 1 && !strcmp(keys[indexNodes], keys[indexs[ind + 1]])){

      // To remove repetitive nodes only the most to the left stay
      if (indexNodes > indexs[ind + 1])
        indexs[ind+1] = indexNodes;

      indexs[ind] = num_pairs;
    }

    if (is_locked[indexList] == 0){
      if (hash_table_list_wrlock(indexList)) continue;

      is_locked[indexList] = 1;
    }
  }

  for(size_t ind = 0; ind < num_pairs; ind++) {
    size_t indexNodes = indexs[ind]; // index of the node to write

    if (indexNodes == num_pairs) continue; // skip same key with with low ind

    aux_message = notif_message;
    strncpy(aux_message, keys[indexNodes], MAX_STRING_SIZE + 1);

    aux_message += MAX_STRING_SIZE + 1;
    strncpy(aux_message, values[indexNodes], MAX_STRING_SIZE + 1);

    // Try to write the key value pair to the hash table
    if ((error = write_pair(kvs_table, keys[indexNodes], values[indexNodes], notif_message)) == -1) {
      fprintf(stderr, "Failed to write keypair (%s,%s)\n", keys[indexNodes],\
        values[indexNodes]);
    }
  }
  // Unlock the hash table's index list that have been locked
  for (size_t ind = 0; ind < 26; ind++)
    if (is_locked[ind]) hash_table_list_unlock(ind);

  hash_table_unlock();
  free(indexs);
  return 0;
}



int kvs_read(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  int is_locked[26] = {0};      // To indicate which indices are lock
  char buffer[MAX_WRITE_SIZE];  // Buffer to store the read content
  size_t *indexs;               // index of each par

  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return -1;
  }
  if (write_error_check(fd, "[") == -1) {
      return -1;
  }
  indexs = malloc(num_pairs * sizeof(size_t));
  if (indexs == NULL) {
    fprintf(stderr, "Failed to allocate memory for indexs\n");
    return -1;
  }

  for (size_t i = 0; i < num_pairs; i++) indexs[i] = i;

  insertion_sort(indexs, num_pairs, keys); // Sort the indexs based on the keys
  // Iterate over the pairs
  for (size_t i = 0; i < num_pairs; i++) {
    size_t index = indexs[i]; // index of the node to read
    size_t indexList = (size_t) hash(keys[index]);

    if (is_locked[indexList] == 0){
      if (hash_table_list_rdlock(indexList)) continue;
      is_locked[indexList] = 1;
    }
  }

  for (size_t i = 0; i < num_pairs; i++) {
    size_t index = indexs[i]; // index of the node to read
    // Try to read the key value pair from the hash table
    char* result = read_pair(kvs_table, keys[index]);
    if (result == NULL) {
      snprintf(buffer, MAX_WRITE_SIZE, "(%s,KVSERROR)", keys[index]);
      if (write_error_check(fd, buffer) == -1) {
        free(indexs);

        // Unlock the hash table's index list that have been locked
        for (size_t ind = 0; ind < 26; ind++)
          if (is_locked[ind]) hash_table_list_unlock(ind);
        return -1;
      }
    } else {
      snprintf(buffer, MAX_WRITE_SIZE, "(%s,%s)", keys[index], result);
      if (write_error_check(fd, buffer) == -1) {
        free(indexs);

        // Unlock the hash table's index list that have been locked
        for (size_t ind = 0; ind < 26; ind++)
          if (is_locked[ind]) hash_table_list_unlock(ind);
        return -1;
      }
    }
    free(result);
  }
  if (write_error_check(fd, "]\n") == -1) {
    free(indexs);
    // Unlock the hash table's index list that have been locked
    for (size_t ind = 0; ind < 26; ind++)
      if (is_locked[ind]) hash_table_list_unlock(ind);
    return -1;
  }
  free(indexs);
  // Unlock the hash table's index list that have been locked
  for (size_t ind = 0; ind < 26; ind++)
    if (is_locked[ind]) hash_table_list_unlock(ind);
  return 0;
}


int kvs_delete(int fd, size_t num_pairs, char keys[][MAX_STRING_SIZE]) {
  int is_locked[26] = {0}; // To indicate witch indices have been locked
  size_t *indexs;          // index of each par
  int aux = 0;
  int ret = 0;

  char notif_message[MAX_STRING_SIZE * 2 + 2] = {0};
  char *aux_message;

  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return -1;
  }

  indexs = malloc(num_pairs * sizeof(size_t));
  // Check if the memory was allocated successfully
  if (indexs == NULL) {
    fprintf(stderr, "Failed to allocate memory for indexs\n");
    return -1;
  }

  // Fill the indexs with the indexs of the pairs
  for (size_t i = 0; i < num_pairs; i++) {
    indexs[i] = i;
  }

  // Lock the hash table for reading
  if (hash_table_rdlock()) return -1;

  // Sort the indexs based on the keys
  insertion_sort(indexs, num_pairs, keys);
  char buffer[MAX_WRITE_SIZE];
  for (size_t i = 0; i < num_pairs; i++) {
    size_t indexNodes = indexs[i];
    size_t indexList = (size_t) hash(keys[indexNodes]);

    if (is_locked[indexList] == 0){
      // Lock the index list for writing
      if (hash_table_list_wrlock(indexList)) continue;

      is_locked[indexList] = 1;
    }
  }

  for (size_t i = 0; i < num_pairs; i++) {
    size_t indexNodes = indexs[i];

    aux_message = notif_message;
    strncpy(aux_message, keys[indexNodes], MAX_STRING_SIZE + 1);

    aux_message += MAX_STRING_SIZE + 1;
    strncpy(aux_message, "DELETED", MAX_STRING_SIZE + 1);

    if (delete_pair(kvs_table, avl_sessions, keys[indexNodes], notif_message) != 0) {
      snprintf(buffer, MAX_WRITE_SIZE, "(%s,KVSMISSING)", keys[indexNodes]);
      if (!aux) {
        if (write_error_check(fd, "[") == -1){
          ret = 1;
          break;
        }
        aux = 1;
      }
      if (write_error_check(fd, buffer) == -1){
          ret = 1;
          break;
        }
    }
  }
  if (aux) {
    if (write_error_check(fd, "]\n") == -1) ret = -1;
  }

  // Unlock the hash table's index list that have been locked
  for (size_t ind = 0; ind < 26; ind++)
    if (is_locked[ind]) hash_table_list_unlock(ind);

  hash_table_unlock();
  free(indexs);
  return ret;
}


int kvs_show(int fd) {

  if (hash_table_wrlock()) return -1;

  for (int i = 0; i < TABLE_SIZE; i++) {
    char buffer[MAX_WRITE_SIZE]; // Buffer to store the show content
    IndexList *indexList = kvs_table->table[i]; // Get the index list
    KeyNode *key_node;

    if (indexList == NULL) continue;

    key_node = indexList->head; // Get the entry's first key node
    // Iterate over the key nodes
    while (key_node != NULL) {
      snprintf(buffer, MAX_WRITE_SIZE, "(%s, %s)\n", key_node->key, \
      key_node->value);

      //Try to write the key value pair to the file descriptor
      if (write_error_check(fd, buffer) == -1) {
        pthread_rwlock_unlock(&kvs_table->rwl);
        return -1;
      }
      key_node = key_node->next; // Move to the next node
    }
  }
  pthread_rwlock_unlock(&kvs_table->rwl);
  return 0;
}


int kvs_backup(int fd) {
  char buffer[MAX_WRITE_SIZE]; // Buffer to store the backup content

  // Iterate over the hash table
  for (int i = 0; i < TABLE_SIZE; i++) {
    IndexList *indexList = kvs_table->table[i]; // Get the index list
    KeyNode *key_node;

    if (indexList == NULL) continue;

    key_node = indexList->head; // First node of the hash table's entry list

    // Iterate over the key nodes
    while (key_node != NULL) {
      char *key = key_node->key;
      char *value = key_node->value;
      char *buf_ptr = buffer;

      *buf_ptr++ = '(';

      // Write the key to the buffer
      while(*key && buf_ptr < buffer + MAX_WRITE_SIZE - 4)
        *buf_ptr++ = *key++;

      *buf_ptr++ = ',';
      *buf_ptr++ = ' ';

      // Write the value to the buffer
      while(*value && buf_ptr < buffer + MAX_WRITE_SIZE - 2)
        *buf_ptr++ = *value++;

      *buf_ptr++ = ')';
      *buf_ptr++ = '\n';
      *buf_ptr++ = '\0';

      // Write the buffer to the file
      if (write_error_check(fd, buffer) == -1) return -1;

      // Move to the next node
      key_node = key_node->next;
    }
  }
  return 0;
}


int kvs_remove_subscription(int client_id, char *key) {
  size_t indexList = (size_t) hash(key);

  if(hash_table_rdlock()) return -1;

  if(hash_table_list_rdlock(indexList)){
    hash_table_unlock();
    return -1;
  }

  KeyNode *key_node = kvs_table->table[indexList]->head;

  if (key_node == NULL){
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  while (key_node != NULL) {
    if (strcmp(key_node->key, key) == 0) {
      avl_remove(key_node->avl_notif_fds, &client_id);

      remove_key_session_avl(client_id, key);
      dec_num_subs(client_id);

      hash_table_list_unlock(indexList);
      hash_table_unlock();
      return 0;
    }
    key_node = key_node->next; // Move to the next node
  }

  hash_table_list_unlock(indexList);
  hash_table_unlock();
  return -1;
}


int kvs_disconnect(int client_id){
  AVL *avl = get_avl_client(client_id);

  if(apply_to_all_nodes(avl, kvs_remove_subscription, client_id) != 0)
    return -1;

  //clean_session_avl(client_id);

  return 0;
}


int kvs_subscribe(int client_id, int notif_fd, char *key){
  size_t indexList = (size_t) hash(key);

  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return -1;
  }

  if (hash_table_rdlock()) return -1;

  if (hash_table_list_rdlock(indexList)){
    hash_table_unlock();
    return -1;
  }

  if(get_avl_num_subs(client_id) == MAX_NUMBER_SUB){
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  if(has_key(get_avl_client(client_id), key)){
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return 0;
  }

  if (subscribe_pair(kvs_table, key, client_id, notif_fd) != 0) {
    fprintf(stderr, "Failed to subscribe client %d to key %s.\n", client_id,\
    key);
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  if (add_key_session_avl(client_id, key) != 0) {
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  inc_num_subs(client_id);

  hash_table_list_unlock(indexList);
  hash_table_unlock();
  return 0;
}


int kvs_unsubscribe(int client_id, char *key){
  size_t indexList = (size_t) hash(key);

  if (kvs_table == NULL) {
    fprintf(stderr, "KVS state must be initialized\n");
    return -1;
  }

  if (hash_table_rdlock()) return -1;

  if (hash_table_list_rdlock(indexList)){
    hash_table_unlock();
    return -1;
  }

  if(!has_key(get_avl_client(client_id), key)){
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  if (unsubscribe_pair(kvs_table, key, client_id) != 0) {
    fprintf(stderr, "Failed to unsubscribe client %d to key %s.\n", client_id,\
    key);
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  if (remove_key_session_avl(client_id, key) != 0) {
    hash_table_list_unlock(indexList);
    hash_table_unlock();
    return -1;
  }

  dec_num_subs(client_id);

  hash_table_list_unlock(indexList);
  hash_table_unlock();
  return 0;

}


void kvs_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}


/// A safe fork preventing the child process to have any locks on the hash table
/// @return value of the child's pid to parent process and 0 to child process.
pid_t do_fork(){
  if (hash_table_wrlock()) return -1;
  pid_t pid = fork();
  hash_table_unlock();
  return pid;
}
