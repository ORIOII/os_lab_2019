// Задание 5: запускает ./sequential_min_max в отдельном процессе через
// связку fork + exec. Родитель дожидается завершения потомка.

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
  int seed = 42;
  int array_size = 1000;

  if (argc == 3) {
    seed = atoi(argv[1]);
    array_size = atoi(argv[2]);
  }

  char seed_str[16];
  char array_size_str[16];
  snprintf(seed_str, sizeof(seed_str), "%d", seed);
  snprintf(array_size_str, sizeof(array_size_str), "%d", array_size);

  pid_t pid = fork();
  if (pid < 0) {
    printf("Fork failed!\n");
    return 1;
  } else if (pid == 0) {
    execl("./sequential_min_max", "sequential_min_max", seed_str,
          array_size_str, (char *)NULL);
    perror("execl failed");
    return 1;
  } else {
    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) {
      printf("sequential_min_max exited with code %d\n",
             WEXITSTATUS(status));
    }
  }

  return 0;
}