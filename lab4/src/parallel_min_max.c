/*
 * Лабораторная работа №4, задание 1.
 *
 * parallel_min_max из ЛР3, дополненный таймаутом:
 *   ./parallel_min_max --seed 1 --array_size 1000 --pnum 4 [--by_files] [--timeout 10]
 *
 * Если задан --timeout N, то через N секунд родитель получает сигнал SIGALRM
 * (его заводит alarm(N)), после чего посылает всем ещё не завершившимся
 * потомкам сигнал SIGKILL. Пока таймаут не истёк, родитель ждёт потомков
 * неблокирующим waitpid(..., WNOHANG).
 * Если --timeout не задан, программа работает так же, как в ЛР3 (обычный wait).
 *
 * Для демонстрации таймаута можно собрать версию с искусственной задержкой
 * в потомках: make parallel_min_max_slow (там -DCHILD_DELAY=2, и потомок
 * номер i "думает" 2*i секунд перед вычислением).
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

#ifndef CHILD_DELAY
#define CHILD_DELAY 0 /* задержка (сек) на номер потомка — только для демонстрации */
#endif

/* Флаг, который выставляет обработчик SIGALRM.
 * volatile sig_atomic_t — единственный тип, который стандарт разрешает
 * безопасно менять из обработчика сигнала. */
static volatile sig_atomic_t timeout_expired = 0;

static void AlarmHandler(int signum) {
  (void)signum;
  timeout_expired = 1; /* в обработчике — только выставляем флаг */
}

/* Посылает SIGKILL всем потомкам, которые ещё не были "собраны" waitpid */
static void KillChildren(const pid_t *pids, int count) {
  for (int i = 0; i < count; i++) {
    if (pids[i] > 0) {
      if (kill(pids[i], SIGKILL) == 0) {
        printf("Timeout: sent SIGKILL to child #%d (pid %d)\n", i, (int)pids[i]);
      }
    }
  }
}

static int FindChild(const pid_t *pids, int count, pid_t pid) {
  for (int i = 0; i < count; i++)
    if (pids[i] == pid) return i;
  return -1;
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  int timeout = -1; /* -1 — таймаут не задан */
  bool with_files = false;

  while (true) {
    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {"timeout", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed must be a positive number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size must be a positive number\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum must be a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;
          case 4:
            timeout = atoi(optarg);
            if (timeout <= 0) {
              printf("timeout must be a positive number\n");
              return 1;
            }
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" "
           "[--by_files] [--timeout \"sec\"]\n",
           argv[0]);
    return 1;
  }

  if (pnum > array_size) {
    printf("pnum should not be greater than array_size\n");
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;

  /* PID каждого потомка (0 — потомок уже собран waitpid'ом)
   * и признак того, что потомок успел нормально завершиться. */
  pid_t *child_pids = calloc(pnum, sizeof(pid_t));
  bool *finished = calloc(pnum, sizeof(bool));

  // Массив pipe'ов — по одному на каждого потомка.
  int pipefd[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipefd[i]) == -1) {
        printf("Pipe failed!\n");
        return 1;
      }
    }
  }

  // разбиение массива между процессами
  int chunk = array_size / pnum;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        // child process
        if (CHILD_DELAY > 0) sleep(CHILD_DELAY * i); /* имитация долгой работы */

        unsigned int begin = (unsigned int)(i * chunk);
        unsigned int end = (i == pnum - 1) ? (unsigned int)array_size
                                            : begin + (unsigned int)chunk;

        struct MinMax min_max = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[64];
          snprintf(filename, sizeof(filename), "min_max_%d.txt", i);
          FILE *f = fopen(filename, "w");
          if (f == NULL) {
            printf("Could not open file %s for writing\n", filename);
            return 1;
          }
          fprintf(f, "%d %d", min_max.min, min_max.max);
          fclose(f);
        } else {
          close(pipefd[i][0]);  // потомку не нужен конец для чтения
          write(pipefd[i][1], &min_max, sizeof(min_max));
          close(pipefd[i][1]);
        }
        return 0;
      }
      child_pids[i] = child_pid; /* это выполняет только родитель */

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  if (!with_files) {
    // родителю не нужны концы для записи — он только читает
    for (int i = 0; i < pnum; i++) {
      close(pipefd[i][1]);
    }
  }

  /* ---------- Задание 1: таймаут ---------- */
  if (timeout > 0) {
    signal(SIGALRM, AlarmHandler); /* что делать, когда придёт SIGALRM */
    alarm(timeout);                /* через timeout секунд ядро пришлёт SIGALRM */
  }

  bool kill_sent = false;
  while (active_child_processes > 0) {
    int status = 0;
    pid_t pid;

    if (timeout > 0) {
      /* Неблокирующее ожидание: если ни один потомок ещё не завершился,
       * waitpid сразу возвращает 0, и мы можем проверить флаг таймаута. */
      pid = waitpid(-1, &status, WNOHANG);
      if (pid == 0) {
        if (timeout_expired && !kill_sent) {
          KillChildren(child_pids, pnum);
          kill_sent = true;
        }
        usleep(10000); /* 10 мс, чтобы не крутить цикл вхолостую */
        continue;
      }
    } else {
      /* Таймаут не задан — поведение как в ЛР3 */
      pid = wait(&status);
    }

    if (pid == -1) {
      if (errno == EINTR) continue; /* ожидание прервано сигналом — повторяем */
      perror("wait");
      break;
    }

    int idx = FindChild(child_pids, pnum, pid);
    if (idx >= 0) {
      child_pids[idx] = 0; /* собран — больше не трогаем его kill'ом */
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        finished[idx] = true;
      } else if (WIFSIGNALED(status)) {
        printf("Child #%d (pid %d) was terminated by signal %d (%s)\n", idx,
               (int)pid, WTERMSIG(status), strsignal(WTERMSIG(status)));
      }
    }
    active_child_processes -= 1;
  }
  if (timeout > 0) alarm(0); /* отменяем будильник, если все успели раньше */

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;
  int finished_count = 0;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;
    bool got = false;

    if (with_files) {
      char filename[64];
      snprintf(filename, sizeof(filename), "min_max_%d.txt", i);
      if (finished[i]) {
        FILE *f = fopen(filename, "r");
        if (f != NULL) {
          got = (fscanf(f, "%d %d", &min, &max) == 2);
          fclose(f);
        }
      }
      remove(filename);
    } else {
      struct MinMax child_min_max;
      /* у убитого потомка read вернёт 0 (EOF): данных в pipe нет */
      if (finished[i] && read(pipefd[i][0], &child_min_max,
                              sizeof(child_min_max)) == sizeof(child_min_max)) {
        min = child_min_max.min;
        max = child_min_max.max;
        got = true;
      }
      close(pipefd[i][0]);
    }

    if (!got) continue;
    finished_count++;
    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  free(child_pids);
  free(finished);

  if (finished_count == 0) {
    printf("No results: all child processes were killed by timeout\n");
  } else {
    if (finished_count < pnum)
      printf("Partial result: only %d of %d processes finished in time\n",
             finished_count, pnum);
    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
  }
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
