#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"
#include <fcntl.h>

char _req_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
char _resp_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
char _notif_pipe_path[MAX_PIPE_PATH_LENGTH + 1];

int _server_pipe_fd;
int _req_pipe_fd;
int _resp_pipe_fd;
int _notif_pipe_fd;

// create pipes and connect
int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* notif_pipe) {

  char connection_buffer[1 + (MAX_PIPE_PATH_LENGTH + 1) * 3];
  char *cur_buffer_pos;

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

  if ((_server_pipe_fd = open(server_pipe_path, O_WRONLY)) < 0){
    perror("Couldn't open server pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    return 1;
  }
  cur_buffer_pos = connection_buffer;
  *cur_buffer_pos = OP_CODE_CONNECT; ////////////////////////////////////////////////////// É PRECISO DAR HANDLE DO ERRO DO STRNCPY?
  cur_buffer_pos++;

  strncpy(_req_pipe_path, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(cur_buffer_pos, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  cur_buffer_pos += MAX_PIPE_PATH_LENGTH + 1;

  strncpy(_resp_pipe_path, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(cur_buffer_pos, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  cur_buffer_pos += MAX_PIPE_PATH_LENGTH + 1;

  strncpy(_notif_pipe_path, notif_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(cur_buffer_pos, notif_pipe_path, MAX_PIPE_PATH_LENGTH + 1);

  if (write_all(_server_pipe_fd, connection_buffer, 1 + (MAX_PIPE_PATH_LENGTH + 1) * 3) < 0){
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    close(_server_pipe_fd);
    return 1;
  }

  if ((_req_pipe_fd = open(server_pipe_path, O_WRONLY)) < 0){
    perror("Couldn't open client request pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    close(_server_pipe_fd);
    return 1;
  }

  if ((_resp_pipe_fd = open(server_pipe_path, O_RDONLY)) < 0){ //ver se n bloqueia e se tem de ser rdwr
    perror("Couldn't open client response pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    close(_server_pipe_fd);
    close(_req_pipe_fd);
    return 1;
  }

  if ((_notif_pipe_fd = open(server_pipe_path, O_RDONLY)) < 0){ //ver se n bloqueia e se tem de ser rdwr
    perror("Couldn't open client notification pipe.\n");
    unlink(req_pipe_path);
    unlink(resp_pipe_path);
    unlink(notif_pipe_path);
    close(_server_pipe_fd);
    close(_req_pipe_fd);
    close(_resp_pipe_fd);
    return 1;
  }

  return 0;
}

// close pipes and unlink pipe files
int kvs_disconnect(void) {

  if (write_all(_server_pipe_fd, (char) OP_CODE_DISCONNECT, 1 ) < 0)
    return 1;

  unlink(_req_pipe_path);
  unlink(_resp_pipe_path);
  unlink(_notif_pipe_path);
  close(_server_pipe_fd);
  close(_req_pipe_fd);
  close(_resp_pipe_fd);
  close(_notif_pipe_fd);
  return 0;
}


// send subscribe message to request pipe and wait for response in response pipe
int kvs_subscribe(const char* key) {
  char subscribe_buffer[MAX_STRING_SIZE + 2]; // Não sei se devemos colocar 1 ou 2 por causa do null terminator da key

  *subscribe_buffer = OP_CODE_SUBSCRIBE;
  strncpy(subscribe_buffer + 1, key, MAX_STRING_SIZE + 1); // Não sei se devemos colocar 1 ou 2 por causa do null terminator da key

  if (write_all(_server_pipe_fd, subscribe_buffer, MAX_STRING_SIZE + 2) < 0)
    return 1;
  return 0;
}


// send unsubscribe message to request pipe and wait for response in response pipe
int kvs_unsubscribe(const char* key) {
  char unsubscribe_buffer[MAX_STRING_SIZE + 2]; // Não sei se devemos colocar 1 ou 2 por causa do null terminator da key

  *unsubscribe_buffer = OP_CODE_UNSUBSCRIBE;
  strncpy(unsubscribe_buffer + 1, key, MAX_STRING_SIZE + 1); // Não sei se devemos colocar 1 ou 2 por causa do null terminator da key

  if (write_all(_server_pipe_fd, unsubscribe_buffer, MAX_STRING_SIZE + 2) < 0)
    return 1;
  return 0;
}


