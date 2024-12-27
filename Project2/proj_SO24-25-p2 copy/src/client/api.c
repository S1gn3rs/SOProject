#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <fcntl.h>

char _req_pipe_path;
char _resp_pipe_path;
char _notif_pipe_path;

int _req_pipe_fd;
int _resp_pipe_fd;
int _notif_pipe_fd;

// create pipes and connect
int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* notif_pipe) {

  char connection_buffer[1 + (MAX_PIPE_PATH_LENGTH + 1) * 3];
  char *cur_buffer_pos;
  int server_pipe_fd;
  int req_pipe_fd, resp_pipe_fd, notif_pipe_fd;

  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);

  if (mkfifo(req_pipe_path, 0666) < 0){
    perror("Couldn't create request pipe.\n");
    return 1;
  }

  if (mkfifo(resp_pipe_path, 0666) < 0){
    unlink(req_pipe_path);
    perror("Couldn't create response pipe.\n");
    return 1;
  }

  if (mkfifo(notif_pipe_path, 0666) < 0){
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    perror("Couldn't create notifications pipe.\n");
    return 1;
  }

  if ((server_pipe_fd = open(server_pipe_path, O_WRONLY)) < 0){
    perror("Couldn't open server pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    return 1;
  }
  cur_buffer_pos = connection_buffer;
  *cur_buffer_pos = OP_CODE_CONNECT; ////////////////////////////////////////////////////// É PRECISO DAR HANDLE DO ERRO DO STRNCPY?
  cur_buffer_pos++;

  strncpy(cur_buffer_pos, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  cur_buffer_pos += MAX_PIPE_PATH_LENGTH + 1;

  strncpy(cur_buffer_pos, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  cur_buffer_pos += MAX_PIPE_PATH_LENGTH + 1;

  strncpy(cur_buffer_pos, notif_pipe_path, MAX_PIPE_PATH_LENGTH + 1);

  if (write_all(server_pipe_fd, connection_buffer, 1 + (MAX_PIPE_PATH_LENGTH + 1) * 3)){
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    return 1;
  }

  if ((req_pipe_fd = open(server_pipe_path, O_WRONLY)) < 0){
    perror("Couldn't open client request pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    return 1;
  }

  if ((resp_pipe_fd = open(server_pipe_path, O_RDONLY)) < 0){ //ver se n bloqueia e se tem de ser rdwr
    perror("Couldn't open client response pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    return 1;
  }

  if ((notif_pipe_fd = open(server_pipe_path, O_RDONLY)) < 0){ //ver se n bloqueia e se tem de ser rdwr
    perror("Couldn't open client notification pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    return 1;
  }

  return 0;
}


int kvs_disconnect(void) {
  // close pipes and unlink pipe files
  return 0;
}

int kvs_subscribe(const char* key) {
  // send subscribe message to request pipe and wait for response in response pipe
  return 0;
}

int kvs_unsubscribe(const char* key) {
    // send unsubscribe message to request pipe and wait for response in response pipe
  return 0;
}


