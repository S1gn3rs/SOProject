#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>

#include "constants.h"
#include "parser.h"
#include "operations.h"
#include <sys/stat.h>

int main(int argc, char *argv[]) {

  if (argc < 2 || argc > 4){ // Confirmar se é para manter para stdin  < 2 // check the tab it will add space inside message error
    fprintf(stderr, "Incorrect arguments.\n Correct use: %s\
    <jobs_directory> [concurrent_backups] [max_threads]\n", argv[0]);
    return 1;
  }

  //unsigned int max_backups = 0; ///////////////////////////////////////////////////////////////////////////////


  DIR *directory = opendir(argv[1]);
  size_t length_dir_name = strlen(argv[1]); // Se ele não der um diretorio dá erro?

  if (directory == NULL){
    perror("Error while trying to open directory\n");
    return 1;
  }

  struct dirent *entry; // SHOULD WE DECLARE THIS HERE OR AT MAIN START

  if (kvs_init()) {
    fprintf(stderr, "Failed to initialize KVS\n");
    return 1;
  }

  while ((entry = readdir(directory)) != NULL) {
    char keys[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    char values[MAX_WRITE_SIZE][MAX_STRING_SIZE] = {0};
    unsigned int delay;
    size_t num_pairs;
    size_t length_entry_name = strlen(entry->d_name);

    // entry->d_type == DT_REG TODO CHECK IF WE CAN ADD THIS AND THE LINKS TOO-------
    if (strcmp(entry->d_name + strlen(entry->d_name) - 4, ".job") != 0) continue; //ver se podemos tirar strlen
    size_t size_in_path = length_dir_name + length_entry_name + 2;
    size_t size_out_path = length_dir_name + length_entry_name + 2;
    char in_path[size_in_path];
    char out_path[size_out_path];

    snprintf(in_path, size_in_path, "%s/%s", argv[1], entry->d_name);
    snprintf(out_path, size_out_path, "%s/%.*s.out", argv[1], (int)length_entry_name - 4, entry->d_name); // este type cast devia de ficar aqui ou nao??????

    int in_fd = open(in_path, O_RDONLY);
    if(in_fd == -1){
      perror("File could not be open.\n");
    }

    int out_fd = open(out_path, O_CREAT | O_TRUNC | O_WRONLY , S_IRUSR | S_IWUSR);
    if(in_fd == -1){
      perror("File could not be open.\n");
    }
  
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
              perror("Error writing");
              return 1;
            }
            kvs_wait(delay);
          }
          break;

        case CMD_BACKUP:
          if (kvs_backup()) {
            fprintf(stderr, "Failed to perform backup.\n");
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

        case EOC: // acho que isto nao e preciso, seria um EOF
          reading_commands = 0;
      }
    }
  close(in_fd);
  close(out_fd);
  }
closedir(directory);
kvs_terminate();
return 0;
}
