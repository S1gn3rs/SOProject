#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"//Clients struct
#include "../common/constants.h"
#include "../common/protocol.h"
#include "../common/safeFunctions.h"

// Global mutex to protect the access to the directory.
pthread_mutex_t mutex;

// Struct to pass arguments to the job thread function.
typedef struct JobThreadArgs {
  char *dir_name;           // Directory name.
  size_t dir_length;        // Directory name length.
}JobThreadArgs;


//Clients struct
typedef struct ClientData {
  char req_pipe_path[MAX_PIPE_PATH_LENGTH];   // Request pipe path.
  char resp_pipe_path[MAX_PIPE_PATH_LENGTH];  // Response pipe path.
  char notif_pipe_path[MAX_PIPE_PATH_LENGTH]; // Notification pipe path.////////////////////////////////////////////////////77777
  char subscribed_keys[MAX_STRING_SIZE][MAX_NUMBER_SUB]; //////////////////////////////////////////////worth dinamico
  int session_id;
}ClientData;


//Client Node
typedef struct ClientNode{
  struct ClientData data;
  struct ClientNode* next;
}ClientNode;


//Queue struct, implemented as a linked list
typedef struct Queue{
  struct ClientNode* head;
  struct ClientNode* tail;
  size_t size; //////////////////////////////////////////////////////////////////////////////77777
}Queue;


int max_backups = 1;          // Max number of concurrent backups.
int active_backups = 0;       // Number of active backups.
DIR *directory;               // Directory to process.

Queue queue = {NULL, NULL, 0};

pthread_mutex_t queue_mutex; // Global mutex to protect the changes on the queue.

sem_t sem_remove_from_queue;
sem_t sem_add_to_queue;



// Thread function to process the .job files.
void *do_commands(void *args) {
  struct dirent *entry;       // Directory entry.
  size_t length_entry_name;   // Get the file name len.

  // Lock the mutex to read the directory.
  if (pthread_mutex_lock(&mutex)){
    fprintf(stderr, "Error trying to lock a mutex\n");
    return NULL;
  }
  while ((entry = readdir(directory)) != NULL) {

    pthread_mutex_unlock(&mutex);

    length_entry_name = strlen(entry->d_name);

    // Check if the file is a .job file if not continue to the next file.
    if (length_entry_name < 4 || strcmp(entry->d_name + length_entry_name - 4,\
    ".job") != 0){

      if (pthread_mutex_lock(&mutex)){
        fprintf(stderr, "Error trying to lock a mutex\n");
        return NULL;
      }
      continue;
    }

    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char in_path[MAX_JOB_FILE_NAME_SIZE];
    char out_path[MAX_JOB_FILE_NAME_SIZE];
    char bck_path[MAX_JOB_FILE_NAME_SIZE];
    unsigned int delay;   // Delay of the WAIT command.
    size_t num_pairs;     // Number of pairs.
    JobThreadArgs *thread_args = (JobThreadArgs*) args;

    // Get the name of the current file.
    char *entry_name = entry->d_name;
    // Get the size of the current path.
    size_t size_path = thread_args->dir_length + length_entry_name + 2;

    // Create the path for the input file.
    snprintf(in_path, size_path, "%s/%s", thread_args->dir_name, entry_name);
    // Create the path for the output file.
    snprintf(out_path, size_path, "%s/%.*s.out", thread_args->dir_name,\
      (int)length_entry_name - 4, entry_name);

    // Open the input file.
    int in_fd = open(in_path, O_RDONLY);
    if(in_fd == -1){
      perror("Input file could not be open\n");
      return NULL;
    }

    // Open the output file.
    int out_fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY ,\
      S_IRUSR | S_IWUSR);

    if(out_fd == -1){
      close(in_fd);
      perror("Output file could not be open\n");
      return NULL;
    }

    // Number of backups made in the current file.
    int backups_made = 0;

    int reading_commands = 1; // flag to let commands from the file.
    while(reading_commands){
      // Get the next command.
      switch (get_next(in_fd)) {
        case CMD_WRITE:
          num_pairs = parse_write(in_fd, keys, values, MAX_WRITE_SIZE,\
            MAX_STRING_SIZE);
          if (num_pairs == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (kvs_write(num_pairs, keys, values)) {
            fprintf(stderr, "Failed to write pair\n");
          }

          break;

        case CMD_READ:
          num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE,\
            MAX_STRING_SIZE);

          if (num_pairs == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (kvs_read(out_fd, num_pairs, keys)) {
            fprintf(stderr, "Failed to read pair\n");
          }
          break;

        case CMD_DELETE:
          num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE,\
            MAX_STRING_SIZE);

          if (num_pairs == 0) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (kvs_delete(out_fd, num_pairs, keys)) {
            fprintf(stderr, "Failed to delete pair\n");
          }
          break;

        case CMD_SHOW:
          kvs_show(out_fd);
          break;

        case CMD_WAIT:
          if (parse_wait(in_fd, &delay, NULL) == -1) {
            fprintf(stderr, "Invalid command. See HELP for usage\n");
            continue;
          }

          if (delay > 0) {
            if (write(out_fd, "Waiting...\n", 11) == -1) {
              perror("Error writing.\n");
            }
            kvs_wait(delay);
          }
          break;

        case CMD_BACKUP:
          // Lock the mutex to check if we can make a backup.
          if (pthread_mutex_lock(&mutex)){
            fprintf(stderr, "Error trying to lock a mutex\n");
            continue;
          }

          // Check if the number of active backups is less than the
          //maximum number of backups.
          if (active_backups >= max_backups) {
            int status;
            if (wait(&status) == -1) {
              perror("Error waiting for backup to be finished.");
            }
          }
          else active_backups++;
          backups_made++;

          // Unlock the mutex after checking if we can make a backup.
          pthread_mutex_unlock(&mutex);
          // Create safely a child process to make the backup.
          pid_t pid = do_fork();
          // Check if the child process was created successfully.
          if(pid == -1){
            fprintf(stderr, "Failed to create child process\n");
            continue;
          }
          if (pid == 0){ // Check if we are in the child process.
            // Create the path for the backup file.
            snprintf(bck_path, size_path + 3, "%s/%.*s-%d.bck",\
              thread_args->dir_name, (int)length_entry_name - 4, entry_name,\
              backups_made);
            // Open the backup file.
            int bck_fd = open(bck_path, O_CREAT | O_TRUNC | O_WRONLY ,\
              S_IRUSR | S_IWUSR);
            // Check if the backup file was opened successfully.
            if(bck_fd == -1) perror("File could not be open.\n");
            else{
              if (kvs_backup(bck_fd))
                fprintf(stderr, "Failed to perform backup\n");
            }
            kvs_terminate();
            close(in_fd);
            close(out_fd);
            closedir(directory);
            exit(0);
          }
          break;

        case CMD_INVALID:
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          break;

        case CMD_HELP:
          printf(
              "Available commands:\n"
              "  WRITE [(key,value)(key2,value2),...]\n"
              "  READ [key,key2,...]\n"
              "  DELETE [key,key2,...]\n"
              "  SHOW\n"
              "  WAIT <delay_ms>\n"
              "  BACKUP\n"
              "  HELP\n"
          );

          break;

        case CMD_EMPTY:
          break;

        case EOC:
          reading_commands = 0;
      }
    }
    close(in_fd);
    close(out_fd);
    // Lock the mutex to read the directory.
    if (pthread_mutex_lock(&mutex)){
      fprintf(stderr, "Error trying to lock a mutex\n");
      continue;
    }
  }
  // Unlock the mutex after thread has no more files to process.
  pthread_mutex_unlock(&mutex);
  return NULL;
}


void *client_thread(void *args) {

  while(1){
    ClientNode* client;

    sem_wait(&sem_remove_from_queue);
    pthread_mutex_lock(&queue_mutex);

    client = queue.head;
    queue.head = client->next;
    if (queue.head == NULL) {
      queue.tail = NULL;
    }
    queue.size--;

    pthread_mutex_unlock(&queue_mutex);
    sem_post(&sem_add_to_queue);

    char response_connection[2]; // OP_CODE=1| result
    response_connection[0] = OP_CODE_CONNECT;
    response_connection[1] = '1'; // Result starts as 1 and changes to 0 on success.

    int resp_pipe_fd = open(client->data.resp_pipe_path, O_WRONLY);
    if (resp_pipe_fd == -1){
      perror("Error opening client response pipe");
      return NULL;
    }

    int req_pipe_fd = open(client->data.req_pipe_path, O_RDONLY);
    if (req_pipe_fd == -1){
      if (write_all(resp_pipe_fd, response_connection, sizeof(char) * 2) == -1) { ////////////////////////////// verificar
        perror("Error writing OP_CODE and result to response pipe");
      }
      perror("Error opening client request pipe");
      return NULL;
    }

    response_connection[1] = '0';

    if (write_all(resp_pipe_fd, response_connection, sizeof(char) * 2) == -1) { ////////////////////////////// verificar
        perror("Error writing OP_CODE and result to response pipe");
    }

    int client_connected = 1;

    while(client_connected){

      char op_code;
      int interrupted_read;
      int read_output;

      if ((read_output = read_all(req_pipe_fd, op_code, sizeof(char), interrupted_read)) < 0){
        perror("Couldn't read message from client."); ////////////////////////////////////////////////////// Aqui é suposto terminar o server do tipo se alguem removeu o fifo que ele tinha criado antes?
        break;
      }else if(read_output == 0){
        perror("Got EOF while trying to read message from client.");
        break;
      }

      switch(op_code){

        case OP_CODE_DISCONNECT:

          close(req_pipe_fd);
          close(resp_pipe_fd);
          free(client);
          client_connected = 0;
          break;

        case OP_CODE_SUBSCRIBE:

          break;

        case OP_CODE_UNSUBSCRIBE:

          break;

      }
    }
  }
}



int main(int argc, char *argv[]) {

  // Check if the number of arguments is correct.
  if (argc != 5){
    fprintf(stderr, "Incorrect arguments.\n Correct use: %s\
    <jobs_directory> <concurrent_backups> <max_threads> <server_FIFO_name>\n",\
    argv[0]);
    return -1;
  }

  size_t length_dir_name = strlen(argv[1]);
  int server_pipe_fd;
  int max_threads;
  // pipe name length plus "/tmp/" and null terminator
  char server_pipe_path[MAX_PIPE_PATH_LENGTH + 6];

  pthread_t job_threads[max_threads];           // Job thread pool.
  pthread_t client_threads[MAX_SESSION_COUNT];  // Client thread pool.
  int job_threads_error[max_threads];
  int client_threads_error[MAX_SESSION_COUNT];

  JobThreadArgs *job_args;



  // Open the directory.
  directory = opendir(argv[1]);

  // Check if the directory was opened successfully.
  if (directory == NULL){
    perror("Error while trying to open directory\n");
    return -1;
  }

  // Check if the number of concurrent backups is valid.
  if( (max_backups = atoi(argv[2])) < 1){
      perror("Number of concurrent backups not valid.\n");
      closedir(directory); // Close the directory in case of error.
      return -1;
  }

  // Check if the number of threads is valid.
  if( (max_threads = atoi(argv[3])) < 1){
      perror("Number of threads not valid.\n");
      closedir(directory); // Close the directory in case of error.
      return -1;
  }

  // Prefix of "/tmp/" in order to change pipe location to /tmp/
  snprintf(server_pipe_path, MAX_PIPE_PATH_LENGTH+6, "%s%s", "/tmp/", argv[4]);

  unlink(server_pipe_path); // If it already exists delete to create a new one.

  if (mkfifo(server_pipe_path, 0666) < 0){
    perror("Couldn't create server pipe.\n");
    closedir(directory); // Close the directory in case of error.
    return -1;
  }

  // Open server pipe in RDWR to make it stay open even without clients.
  if ((server_pipe_fd = open(server_pipe_path, O_RDWR)) < 0){
    perror("Couldn't open server pipe.\n");
    unlink(server_pipe_path);     // Close the server pipe in case of error.
    closedir(directory); // Close the directory in case of error.
    return -1;
  }

  // Initialize the KVS.
  if (kvs_init()) {
    fprintf(stderr, "Failed to initialize KVS\n");
    close(server_pipe_fd);
    unlink(server_pipe_path);     // Close the server pipe in case of error.
    closedir(directory);
    return -1;
  }

  // Initialize the global mutex
  if(pthread_mutex_init(&mutex, NULL)){
    fprintf(stderr, "Failed to initialize the mutex\n");
    close(server_pipe_fd);
    unlink(server_pipe_path);     // Close the server pipe in case of error.
    closedir(directory);
    return -1;
  }



  // Struct to pass arguments to the thread function.
  job_args = malloc(sizeof(JobThreadArgs));
  if (!job_args){
    unlink(server_pipe_path);     // Close the server pipe in case of error.
    closedir(directory);
    kvs_terminate();
    pthread_mutex_destroy(&mutex);
    return -1;
  }

  // Assign struct attributes.
  job_args->dir_length = length_dir_name;
  job_args->dir_name = argv[1];

  // Create the threads.
  for(int thread_count = 0; thread_count < max_threads; thread_count++){
    if (pthread_create(&job_threads[thread_count], NULL, do_commands,\
      (void *)job_args) != 0) {

      fprintf(stderr, "Error: Unable to create job thread %d.\n", thread_count);
      job_threads_error[thread_count] = 1;
    }
    else job_threads_error[thread_count] = 0;
  }

  // Free the arguments struct.
  free(job_args);



  for(int thread_count = 0; thread_count < MAX_SESSION_COUNT; thread_count++){
    if (pthread_create(&client_threads[thread_count], NULL, client_thread,\
    (void *)args) != 0){

      fprintf(stderr, "Error: Unable to create client thread %d.\n", thread_count);
      client_threads_error[thread_count] = 1;
    }
    else client_threads_error[thread_count] = 0;
  }


  sem_init(&sem_add_to_queue, 0, MAX_SESSION_COUNT);
  sem_init(&sem_remove_from_queue, 0, 0);

  pthread_mutex_init(&queue_mutex, NULL);

  int idClient = 1;

  while (1){
    int length_client_con = 1 + (MAX_PIPE_PATH_LENGTH + 1) * 3;
    char client_connection[length_client_con]; /////////////////////CONFIRMAR SE É SUPOSTO TER MAIS 1 PARA O \00 OU NAO ////////////////////////7
    char *aux_client_conn;
    int interrupted_read;////////////////// dar handle aqui e na api tmb ///////////////////////////////////////////////////////////////
    int read_output;
    ClientNode *client;
    ClientData *data;

    if ((read_output = read_all(server_pipe_fd, client_connection, length_client_con, interrupted_read)) < 0){
      perror("Couldn't read connection message from client."); ////////////////////////////////////////////////////// Aqui é suposto terminar o server do tipo se alguem removeu o fifo que ele tinha criado antes?
      break;
    }
    // Not expected due to how the fifo was opened but included as a precaution.
    else if(read_output == 0){
      perror("Got EOF while trying to read message from client.");
      break;
    }

    if (*client_connection != OP_CODE_CONNECT){
      fprintf(stderr, "Message from client had OPCODE: %c instead of OPCODE: 1.\n", OP_CODE_CONNECT);
      continue;
    }

    client = malloc(sizeof(ClientNode));
    if (client == NULL){
      perror("Could not allocate memory to client structure.");
      continue;
    }

    client->next = NULL;
    client->data.session_id = idClient;
    aux_client_conn = client_connection + 1;
    data = &client->data;
    safe_strncpy(data->req_pipe_path, aux_client_conn, MAX_PIPE_PATH_LENGTH + 1);

    aux_client_conn += MAX_PIPE_PATH_LENGTH + 1;
    safe_strncpy(data->resp_pipe_path, aux_client_conn, MAX_PIPE_PATH_LENGTH + 1);

    aux_client_conn += MAX_PIPE_PATH_LENGTH + 1;
    safe_strncpy(data->notif_pipe_path, aux_client_conn,MAX_PIPE_PATH_LENGTH + 1);

    sem_wait(&sem_add_to_queue); // If queue is not full let server add 1 client.

    if (queue.head == NULL)
      queue.head = client;
    else
      queue.tail->next = client;

    queue.tail = client;
    queue.size++;


    sem_post(&sem_remove_from_queue); // Let a thread get one client from the queue.

  idClient++;
  }



  // DAQUI PARA BAIXO ACHO QUE NÃO É PRECISO NADA E PELA CONVERSA COM O STOR PODEMOS MANTER OU ELIMINAR --------------------------------------------


  // Wait for the threads to finish.
  for (int i = 0; i < max_threads; i++) {
    if (job_threads_error[i]) continue; // Skip if there was an error
    if (pthread_join(job_threads[i], NULL) != 0) {
      fprintf(stderr, "Error: Unable to join job thread %d.\n", i);
    }
  }

  // for (int i = 0; i < MAX_SESSION_COUNT; i++) {
  //   if (client_threads_error[i]) continue; // Skip if there was an error
  //   if (pthread_join(client_threads[i], NULL) != 0) {
  //     fprintf(stderr, "Error: Unable to join  client thread %d.\n", i);
  //   }
  // }

  // Close the directory.
  closedir(directory);

  unlink(server_pipe_path);     // Close the server pipe in case of error.

  // Destroy the global mutex.
  pthread_mutex_destroy(&mutex);


  // Terminate the KVS.
  kvs_terminate();

  // Wait for the backups to finish.
  while (active_backups-- > 0){
    if(wait(NULL) == -1){
      perror("Error waiting for backup to be finished.");
    }
  };

  return 0;
}
