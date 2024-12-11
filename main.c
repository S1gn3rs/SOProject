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

// Global mutex to protect the access to the directory.
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Struct to pass arguments to the thread function.
typedef struct ThreadArgs {
    char *dir_name; // Directory name.
    size_t dir_length; // Directory name length.
}ThreadArgs;

// Max number of concurrent backups.
int max_backups = 1;
// Number of active backups.
int active_backups = 0;
// Directory to process.
DIR *directory;

// Thread function to process the .job files.
void *thread_function(void *args) {
  // Directory entry.
  struct dirent *entry;

  // Lock the mutex to read the directory.
  pthread_mutex_lock(&mutex);
  while ((entry = readdir(directory)) != NULL) {

    // Unlock the mutex after reading from the directory to process the file.
    pthread_mutex_unlock(&mutex);

    size_t length_entry_name = strlen(entry->d_name);

    // Check if the file is a .job file if not continue to the next file.
    if (length_entry_name < 4 || strcmp(entry->d_name + length_entry_name - 4, ".job") != 0){
      pthread_mutex_lock(&mutex);
      continue;
    }

    // Array to store the keys of the commands.
    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    // Array to store the values of the commands.
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    // Delay of the WAIT command.
    unsigned int delay;
    // Number of pairs.
    size_t num_pairs;

    // Struct to read the arguments passed to the thread function.
    ThreadArgs *threadArgs = (ThreadArgs*) args;

    // Path for the input file.
    char in_path[MAX_JOB_FILE_NAME_SIZE];
    // Path for the output file.
    char out_path[MAX_JOB_FILE_NAME_SIZE];
    // Path for the backup file.
    char bck_path[MAX_JOB_FILE_NAME_SIZE + 3]; // SHOULD BE MORE BC OF NUMBERS??????????????????????????????????????????????????????????????????????????????

    // Get the name of the current file.
    char *entry_name = entry->d_name;
    // Get the size of the current path.
    size_t size_path = threadArgs->dir_length + length_entry_name + 2;

    // Create the path for the input file.
    snprintf(in_path, size_path, "%s/%s", threadArgs->dir_name, entry_name);
    // Create the path for the output file.
    snprintf(out_path, size_path, "%s/%.*s.out", threadArgs->dir_name, (int)length_entry_name - 4, entry_name); // este type cast devia de ficar aqui ou nao??????

    // Open the input file.
    int in_fd = open(in_path, O_RDONLY);
    if(in_fd == -1){
      perror("File could not be open.\n");
      return NULL;
    }

    // Open the output file.
    int out_fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);
    if(in_fd == -1){
      perror("File could not be open.\n");
      return NULL;
    }
    
    // Number of backups made in the current file.
    int backups_made = 0;

    int reading_commands = 1;

    // Read the commands from the file.
    while(reading_commands){
      // Get the next command.
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
          // Lock the mutex to check if we can make a backup.
          pthread_mutex_lock(&mutex);
          // Check if the number of active backups is less than the maximum number of backups.
          if (active_backups >= max_backups) {
              int status;
              wait(&status);// wait for a backup to finish
          }
          // Increment the number of active backups.
          else active_backups++;
          // Increment the number of backups made in the current file.
          backups_made++;
          // Unlock the mutex after checking if we can make a backup.
          pthread_mutex_unlock(&mutex);
          // Create a child process to make the backup.
          pid_t pid = do_fork();
          // Check if the child process was created successfully.
          if(pid == -1){
                  fprintf(stderr, "Failed to create child process\n");
                  exit(1);
          }
          // Check if we are in the child process.
          if (pid == 0){ ///////////////////////////////////////////////////////////preciso de pareteses?
            // Create the path for the backup file.
            snprintf(bck_path, size_path + 3, "%s/%.*s-%d.bck", threadArgs->dir_name, (int)length_entry_name - 4, entry_name, backups_made);
            // Open the backup file.
            int bck_fd = open(bck_path, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);
            // Check if the backup file was opened successfully.
            if(bck_fd == -1){
              perror("File could not be open.\n");
              return NULL;
            }
            // Perform the backup.
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
    // Close the input file.
    close(in_fd);
    // Close the output file.
    close(out_fd);
    // Lock the mutex to read the directory.
    pthread_mutex_lock(&mutex);
  }
  // Unlock the mutex after thread has no more files to process.
  pthread_mutex_unlock(&mutex);
  return NULL;
}

int main(int argc, char *argv[]) {
  // Check if the number of arguments is correct.
  if (argc < 2 || argc > 4){
    fprintf(stderr, "Incorrect arguments.\n Correct use: %s\
    <jobs_directory> [concurrent_backups] [max_threads]\n", argv[0]);
    return 1;
  }

  // Open the directory.
  directory = opendir(argv[1]);

  // Get the length of the directory name.
  size_t length_dir_name = strlen(argv[1]);

  // Check if the directory was opened successfully.
  if (directory == NULL){
    perror("Error while trying to open directory\n");
    return 1;
  }

  // Check if we had more than 2 arguments if the number of concurrent backups is valid.
  if (argc >= 3){
    if( (max_backups = atoi(argv[2])) < 1){
      perror("Number of concurrent backups not valid.\n");
      closedir(directory); // Close the directory in case of error.
      return 1;
    }
  }

  // Default number of threads.
  int max_threads = 1;

  // Check if we had more than 3 arguments if the number of threads is valid.
  if (argc == 4){
    if( (max_threads = atoi(argv[3])) < 1){
      perror("Number of threads not valid.\n");
      closedir(directory); // Close the directory in case of error.
      return 1;
    }
  }

  // Initialize the KVS.
  if (kvs_init()) {
    fprintf(stderr, "Failed to initialize KVS\n");
    closedir(directory);
    return 1;
  }

  // Thread pool.
  pthread_t threads[max_threads];
  // Number of threads created.
  int thread_count = 0;

  // Struct to pass arguments to the thread function.
  ThreadArgs *args = malloc(sizeof(ThreadArgs)); // VERIFICAR SE PRECISAMOS DE LEVANTAR ERRO
  args->dir_length = length_dir_name;
  args->dir_name = argv[1];

  // Create the threads.
  for(thread_count = 0; thread_count < max_threads; thread_count++){
    pthread_create(&threads[thread_count], NULL, thread_function, (void *)args);
  }

  // Wait for the threads to finish.
  for(int i = 0; i < thread_count; i++) {
    pthread_join(threads[i], NULL);
  }

  // Free the arguments struct.
  free(args);

  // Close the directory.
  closedir(directory);

  // Terminate the KVS.
  kvs_terminate();

  // Wait for the backups to finish.
  while (active_backups-- > 0) wait(NULL);

  return 0;
}
