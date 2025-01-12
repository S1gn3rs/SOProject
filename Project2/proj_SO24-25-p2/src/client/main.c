#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "parser.h"
#include "src/client/api.h"
#include "src/common/constants.h"
#include "src/common/io.h"

int client_notifications_fd;

void* notif_function(void *args){
  (void)args;  // Ignore the unused parameter

  ssize_t bytes_read1, bytes_read2;
  char key[MAX_STRING_SIZE + 1];
  char value[MAX_STRING_SIZE + 1];
  char message[MAX_STRING_SIZE * 2 + 5];
  int interrupted_read = 0;

  while (1) {

    bytes_read1 = read_all(client_notifications_fd, key, MAX_STRING_SIZE, &interrupted_read);
    if (bytes_read1 <= 0) {
      if (bytes_read1 == 0) {
        break; // End of file or pipe closed
      }
      perror("Error reading key from notification pipe");
      break;
    }
    key[bytes_read1] = '\0';

    interrupted_read = 0;
    bytes_read2 = read_all(client_notifications_fd, value, MAX_STRING_SIZE, &interrupted_read);
    if (bytes_read2 <= 0) {
      if (bytes_read2 == 0) {
        break; // End of file or pipe closed
      }
      perror("Error reading value from notification pipe");
      break;
    }
    value[bytes_read2] = '\0';

    snprintf(message, sizeof(message), "(%s,%s)\n", key, value);

    if (write_all(STDOUT_FILENO, message, strlen(message)) == -1) {
      perror("Error writing notifications to stdout");
    }
  }

  close(client_notifications_fd); // Close the notification file descriptor when done
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

  pthread_t notif_thread;

  char keys[MAX_NUMBER_SUB][MAX_STRING_SIZE] = {0};
  unsigned int delay_ms;
  size_t num;

  snprintf(server_pipe_path, MAX_PIPE_PATH_LENGTH+6, "%s%s", "/tmp/", argv[2]);
  strncat(req_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(resp_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));
  strncat(notif_pipe_path, argv[1], strlen(argv[1]) * sizeof(char));

  // TODO open pipes

  if (kvs_connect(req_pipe_path, resp_pipe_path, server_pipe_path, notif_pipe_path, &client_notifications_fd) != 0) {
    return 1;
  }

  if (pthread_create(&notif_thread, NULL, notif_function,\
    NULL) != 0){

    fprintf(stderr, "Error: Unable to create notifications thread \n");
  }

  while (1) {
    switch (get_next(STDIN_FILENO)) {
      case CMD_DISCONNECT:
        if (kvs_disconnect() != 0) {
          fprintf(stderr, "Failed to disconnect to the server\n");
          return 1;
        }
        // TODO: end notifications thread
        if (pthread_join(notif_thread, NULL) != 0) {
          fprintf(stderr, "Error: Unable to join notifications thread.\n");
        }
        unlink(notif_pipe_path);
        printf("Disconnected from server\n");
        return 0;

      case CMD_SUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_subscribe(keys[0])) {
            fprintf(stderr, "Command subscribe failed\n");
        }

        break;

      case CMD_UNSUBSCRIBE:
        num = parse_list(STDIN_FILENO, keys, 1, MAX_STRING_SIZE);
        if (num == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_unsubscribe(keys[0])) {
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
