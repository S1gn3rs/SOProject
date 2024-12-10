#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include <sys/stat.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

typedef struct ThreadArgs {
    char *dir_name;
    size_t dir_length;
    struct dirent *entry;
}ThreadArgs;

int active_threads = 0;
int max_backups = 1;
int active_backups = 0;
DIR *directory;

void *thread_function(void *args) {
  char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
  unsigned int delay;
  size_t num_pairs;
  ThreadArgs *threadArgs = (ThreadArgs*) args;

  char in_path[MAX_JOB_FILE_NAME_SIZE];
  char out_path[MAX_JOB_FILE_NAME_SIZE];
  char bck_path[MAX_JOB_FILE_NAME_SIZE + 3]; // SHOULD BE MORE BC OF NUMBERS??????????????????????????????????????????????????????????????????????????????

  char *entry_name = threadArgs->entry->d_name;
  size_t length_entry_name = strlen(entry_name);
  size_t size_path = threadArgs->dir_length + length_entry_name + 2;

  snprintf(in_path, size_path, "%s/%s", threadArgs->dir_name, entry_name);
  snprintf(out_path, size_path, "%s/%.*s.out", threadArgs->dir_name, (int)length_entry_name - 4, entry_name); // este type cast devia de ficar aqui ou nao??????

  int in_fd = open(in_path, O_RDONLY);
  if(in_fd == -1){
    perror("File could not be open.\n");
    return NULL;
  }

  int out_fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);
  if(in_fd == -1){
    perror("File could not be open.\n");
    return NULL;
  }

  //max_backups definido em cima --------------------------------------------------------------
  int backups_made = 0;
  int reading_commands = 1;
  while(reading_commands){

    switch (get_next(in_fd)) {
      case CMD_WRITE:
        num_pairs = parse_write(in_fd, keys, values, MAX_WRITE_SIZE, MAX_STRING_SIZE);
        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_write(num_pairs, keys, values)) {
          fprintf(stderr, "Failed to write pair\n");
        }

        break;

      case CMD_READ:
        num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

        if (num_pairs == 0) {
          fprintf(stderr, "Invalid command. See HELP for usage\n");
          continue;
        }

        if (kvs_read(out_fd, num_pairs, keys)) {
          fprintf(stderr, "Failed to read pair\n");
        }
        break;

      case CMD_DELETE:
        num_pairs = parse_read_delete(in_fd, keys, MAX_WRITE_SIZE, MAX_STRING_SIZE);

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
          if (write(out_fd, "Waiting...\n", 11) == -1) { // 11 is the length of "Waiting...\n" is ok to hardcode this?
            perror("Error writing\n");
            return NULL;
          }
          kvs_wait(delay);//////////////////////////////////////////////////////////////////////////
        }
        break;

      case CMD_BACKUP:
        pthread_mutex_lock(&mutex);
        if (active_backups >= max_backups) {
            int status;
            wait(&status);// É SUPOSTO LIDAR COM ERRO CASO BACKUP FALHE
        }
        else active_backups++;
        backups_made++;
        pthread_mutex_unlock(&mutex);

        pid_t pid = fork();
        if(pid == -1){
                fprintf(stderr, "Failed to create child process\n");
                exit(1);
        }
        if (pid == 0){ ///////////////////////////////////////////////////////////preciso de pareteses?
          // Vai se lixar tudo com treads
          snprintf(bck_path, size_path + 3, "%s/%.*s-%d.bck", threadArgs->dir_name, (int)length_entry_name - 4, entry_name, backups_made);
          int bck_fd = open(bck_path, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);

          if(bck_fd == -1){
            perror("File could not be open.\n");
            return NULL;
          }

          if (kvs_backup(bck_fd)) {
            fprintf(stderr, "Failed to perform backup.\n");
          }
          //sleep(5);
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
            "  BACKUP\n" // Not implemented
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
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cond);
  active_threads--;
  pthread_mutex_unlock(&mutex);
  free(args);
  return NULL;
}


int main(int argc, char *argv[]) {

  if (argc < 2 || argc > 4){ // Confirmar se é para manter para stdin  < 2 // check the tab it will add space inside message error
    fprintf(stderr, "Incorrect arguments.\n Correct use: %s\
    <jobs_directory> [concurrent_backups] [max_threads]\n", argv[0]);
    return 1;
  }

  directory = opendir(argv[1]);
  size_t length_dir_name = strlen(argv[1]);

  if (directory == NULL){
    perror("Error while trying to open directory\n");
    return 1;
  }

  if (argc >= 3){
    if( (max_backups = atoi(argv[2])) < 1){
      perror("Number of concurrent backups not valid.\n");
      closedir(directory);
      return 1;
    }
  }

  int max_threads = 1;
  if (argc == 4){
    if( (max_threads = atoi(argv[3])) < 1){
      perror("Number of threads not valid.\n");
      closedir(directory);
      return 1;
    }
  }

  struct dirent *entry; // SHOULD WE DECLARE THIS HERE OR AT MAIN START

  if (kvs_init()) {
    fprintf(stderr, "Failed to initialize KVS\n");
    closedir(directory);
    return 1;
  }

  pthread_t threads[1024];
  int thread_count = 0;

  while ((entry = readdir(directory)) != NULL) {
    size_t length_entry_name = strlen(entry->d_name);

    if (length_entry_name < 4 || strcmp(entry->d_name + length_entry_name - 4, ".job") != 0) continue;

    ThreadArgs *args = malloc(sizeof(ThreadArgs)); // DESCONTA SE NÃO TIVER TYPE CAST?????????????????
    args->dir_length = length_dir_name;
    args->entry = entry;
    args->dir_name = argv[1];

    pthread_mutex_lock(&mutex);
    while (active_threads >= max_threads){
      pthread_cond_wait(&cond, &mutex);
    }
    active_threads++;
    pthread_mutex_unlock(&mutex);

    pthread_create(&threads[thread_count++], NULL, thread_function, (void *)args);
  }
  for (int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }
  closedir(directory);

  kvs_terminate();
  while (active_backups-- > 0) wait(NULL);

  return 0;
}
