#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>

#include <signal.h>

#include "parser.h"
#include "src/client/api.h"
#include "src/common/constants.h"
#include "src/common/io.h"
#include "src/common/safeFunctions.h"

int client_notifications_fd;

pthread_mutex_t stdout_mutex;

atomic_bool terminate = false;

pthread_t main_thread_id;

void signal_handler(int signum) {
  if (signum == SIGUSR1){};
}

void* notif_function(void *args){
  (void)args;  // Ignore the unused parameter
  // pthread_t main_id = *(pthread_t *)args;

  ssize_t bytes_read;
  char buffer[(MAX_STRING_SIZE + 1) * 2];
  char key[MAX_STRING_SIZE + 1];
  char value[MAX_STRING_SIZE + 1];
  char message[MAX_STRING_SIZE * 2 + 5];
  int interrupted_read = 0;

  while (!atomic_load(&terminate)) {
    errno = 0; // Reset errno before the system call
    bytes_read = read_all(client_notifications_fd, buffer,\
      (MAX_STRING_SIZE + 1) * 2, &interrupted_read);

    if (bytes_read <= 0 || errno == EPIPE) {

      if (pthread_kill(main_thread_id, SIGUSR1) != 0) {
        perror("Failed to send SIGUSR1 to main thread");
      }
      atomic_store(&terminate, true);
      break;
    }

    // Extract key and value from the buffer
    strncpy(key, buffer, MAX_STRING_SIZE + 1);
    key[MAX_STRING_SIZE] = '\0'; // Ensure null termination
    strncpy(value, buffer + MAX_STRING_SIZE + 1, MAX_STRING_SIZE + 1);
    value[MAX_STRING_SIZE] = '\0'; // Ensure null termination

    // Calculate the actual length of key and value (excluding padding)
    size_t key_len = strlen(key);
    size_t value_len = strlen(value);

    // Construct the message (key,value)\n
    snprintf(message, sizeof(message), "(%.*s,%.*s)\n",
             (int)key_len, key, (int)value_len, value);

    if (pthread_mutex_lock(&stdout_mutex)) {
      perror("Error locking stdout mutex");
    }

    if (write_all(STDOUT_FILENO, message, strlen(message)) == -1) {
      perror("Error writing notifications to stdout");
    }

    if (pthread_mutex_unlock(&stdout_mutex)) {
      perror("Error unlocking stdout mutex");
    }

  }
  return NULL;
}


int main(int argc, char* argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <client_unique_id> <register_pipe_path>\n", argv[0]);
    return 1;
  }

  char req_pipe_path[256] = "/tmp/req";
  char resp_pipe_path[256] = "/tmp/resp";
  char notif_pipe_path[256] = "/tmp/notif";
  char server_pipe_path[MAX_PIPE_PATH_LENGTH + 6];

  int req_pipe_fd;
  int resp_pipe_fd;

  pthread_t notif_thread;

  if(pthread_mutex_init(&stdout_mutex, NULL)){
    fprintf(stderr, "Failed to initialize the mutex\n");
    return 1;
  }

  char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
  unsigned int delay_ms;
  size_t num;

  snprintf(server_pipe_path, MAX_PIPE_PATH_LENGTH+6, "%s%s", "/tmp/", argv[2]);
  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  if (kvs_connect(req_pipe_path, resp_pipe_path, server_pipe_path,\
    notif_pipe_path, &req_pipe_fd, &resp_pipe_fd, &client_notifications_fd,\
    &stdout_mutex, &terminate) != 0) {

    pthread_mutex_destroy(&stdout_mutex);
    return 1;
  }

    // Register signal handler for SIGUSR1
  if (signal(SIGUSR1, signal_handler) == SIG_ERR) {
      perror("Failed to set up signal handler");
      return 1;
  }
  // Get the main thread ID
  main_thread_id = pthread_self();


  if (pthread_create(&notif_thread, NULL, notif_function,\
    (void*) &main_thread_id) != 0){

    fprintf(stderr, "Error: Unable to create notifications thread \n");
    pthread_mutex_destroy(&stdout_mutex);
    return 1;
  }


  while (1) {

    if (atomic_load(&terminate)) {

        if (pthread_join(notif_thread, NULL) != 0) {
            fprintf(stderr, "Error: Unable to join notifications thread.\n");
        }

        pthread_mutex_destroy(&stdout_mutex);
        close(req_pipe_fd);
        close(resp_pipe_fd);
        close(client_notifications_fd);
        unlink(req_pipe_path);
        unlink(resp_pipe_path);
        unlink(notif_pipe_path);

        return 0;
    }

    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect(&stdout_mutex, &terminate) != 0) {
          fprintf(stderr, "Failed to disconnect to the server\n");
          return 1;
        }

        if (pthread_join(notif_thread, NULL) != 0) {
          fprintf(stderr, "Error: Unable to join notifications thread.\n");
        }

        pthread_mutex_destroy(&stdout_mutex);
        unlink(notif_pipe_path);
        return 0;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_subscribe(keys[0], &stdout_mutex, &terminate)) {
            fprintf(stderr, "Command subscribe failed\n");
        }
        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_unsubscribe(keys[0], &stdout_mutex, &terminate)) {
            fprintf(stderr, "Command subscribe failed\n");
        }

        break;

      case CMD_DELAY:
        if (parse_delay(STDIN_FILENO, &delay_ms) == -1) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (delay_ms > 0) {
            printf("Waiting...\n");
            delay(delay_ms);
        }
        break;

      case CMD_INVALID:
        fprintf(stderr, "Invalid command. See HELP for usage\n");
        break;

      case CMD_EMPTY:
        break;

      case EOC:
        // input should end in a disconnect, or it will loop here forever
        break;
    }
  }
}
