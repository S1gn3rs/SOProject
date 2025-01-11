#include <fcntl.h>
#include <stdio.h>

#include "api.h"
#include "src/common/constants.h"
#include "src/common/protocol.h"


char api_req_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
char api_resp_pipe_path[MAX_PIPE_PATH_LENGTH + 1];
char api_notif_pipe_path[MAX_PIPE_PATH_LENGTH + 1];

int api_req_pipe_fd;
int api_resp_pipe_fd;

// create pipes and connect
int kvs_connect(char const* req_pipe_path, char const* resp_pipe_path, char const* server_pipe_path,
                char const* notif_pipe_path, int* client_notif_pipe_fd) {
  int length_con_buffer = 1 + (MAX_PIPE_PATH_LENGTH + 1) * 3;
  char connection_buffer[length_con_buffer];
  char *cur_buffer_pos;
  int server_pipe_fd;

  char response_buffer[2];
  int interrupted_read;

  unlink(req_pipe_path);
  unlink(resp_pipe_path);
  unlink(notif_pipe_path);

  if (mkfifo(req_pipe_path, 0666) < 0){
    perror("Couldn't create request pipe.");
    return 1;
  }

  if (mkfifo(resp_pipe_path, 0666) < 0){
    perror("Couldn't create response pipe.");
    return 1;
  }

  // if (mkfifo(notif_pipe_path, 0666) < 0){
  //   perror("Couldn't create notifications pipe.");
  //   return 1;
  // }

  if ((server_pipe_fd = open(server_pipe_path, O_WRONLY)) < 0){
    perror("Couldn't open server pipe.");
    return 1;
  }

  cur_buffer_pos = connection_buffer;
  *cur_buffer_pos = OP_CODE_CONNECT; ////////////////////////////////////////////////////// É PRECISO DAR HANDLE DO ERRO DO STRNCPY?
  cur_buffer_pos++;

  strncpy(api_req_pipe_path, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(cur_buffer_pos, req_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  cur_buffer_pos += MAX_PIPE_PATH_LENGTH + 1;

  strncpy(api_resp_pipe_path, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(cur_buffer_pos, resp_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  cur_buffer_pos += MAX_PIPE_PATH_LENGTH + 1;

  strncpy(api_notif_pipe_path, notif_pipe_path, MAX_PIPE_PATH_LENGTH + 1);
  strncpy(cur_buffer_pos, notif_pipe_path, MAX_PIPE_PATH_LENGTH + 1);

  if (write_all(server_pipe_fd, connection_buffer, length_con_buffer) < 0){
    close(server_pipe_fd);
    return 1;
  }

  close(server_pipe_fd);

  if ((api_req_pipe_fd = open(req_pipe_path, O_WRONLY)) < 0){
    perror("Couldn't open client request pipe.");
    return 1;
  }

  if ((api_resp_pipe_fd = open(resp_pipe_path, O_RDONLY)) < 0){ //ver se n bloqueia e se tem de ser rdwr
    perror("Couldn't open client response pipe.");
    close(api_req_pipe_fd);
    return 1;
  }

  if ((*client_notif_pipe_fd = open(notif_pipe_path, O_RDONLY)) < 0){ //ver se n bloqueia e se tem de ser rdwr
    perror("Couldn't open client notification pipe.");
    close(api_req_pipe_fd);
    close(api_resp_pipe_fd);
    return 1;
  }

  if (read_all(api_resp_pipe_fd, response_buffer, 2, &interrupted_read) < 0){
    perror("Couldn't read message from server.");
    close(api_req_pipe_fd);
    close(api_resp_pipe_fd);
    close(*client_notif_pipe_fd);
    return 1;
  }

  if(response_buffer[1] != '0'){
    perror("Couldn't connect to server.");
    close(api_req_pipe_fd);
    close(api_resp_pipe_fd);
    close(*client_notif_pipe_fd);
  }

  fprintf(stdout, "Server returned %c for operation: connect", response_buffer[1]); //////////checkar se é ok usar fprintf ou se temos de mudar

  return (response_buffer[1] == '0') ? 0 : 1;
}

// close pipes and unlink pipe files
int kvs_disconnect(void) {
  char response_buffer[2];
  int interrupted_read;

  if (write_all(api_req_pipe_fd, OP_CODE_DISCONNECT, 1) < 0)
    return 1;
  /////////////////////////////////////////////// FALTA READ ??????????????????????????????????????????????????????????????????????????????????????
  close(api_req_pipe_fd);
  close(api_resp_pipe_fd);
  unlink(api_req_pipe_path);
  unlink(api_resp_pipe_path);
  return 0;
}


// send subscribe message to request pipe and wait for response in response pipe
int kvs_subscribe(const char* key) {
  char subscribe_buffer[MAX_STRING_SIZE + 2];
  char response_buffer[2];
  int interrupted_read;

  *subscribe_buffer = OP_CODE_SUBSCRIBE;
  strncpy(subscribe_buffer + 1, key, MAX_STRING_SIZE + 1);

  if (write_all(api_req_pipe_fd, subscribe_buffer, MAX_STRING_SIZE + 2) < 0)
    return 1;
  return 0;
}


// send unsubscribe message to request pipe and wait for response in response pipe
int kvs_unsubscribe(const char* key) {
  char unsubscribe_buffer[MAX_STRING_SIZE + 2];
  char response_buffer[2];
  int interrupted_read;

  *unsubscribe_buffer = OP_CODE_UNSUBSCRIBE;
  strncpy(unsubscribe_buffer + 1, key, MAX_STRING_SIZE + 1);

  if (write_  (api_req_pipe_fd, unsubscribe_buffer, MAX_STRING_SIZE + 2) < 0)
    return 1;
  return 0;
}


