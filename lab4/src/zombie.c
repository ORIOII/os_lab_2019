/*
 * Лабораторная работа №4, задание 2. Демонстрация зомби-процессов.
 *
 *   ./zombie [режим] [сек]
 *
 * Режимы:
 *   (без аргумента) / zombie  — потомки завершаются, родитель НЕ вызывает wait():
 *                               потомки висят в состоянии Z (<defunct>).
 *                               Через [сек] секунд (по умолчанию 20) родитель
 *                               вызывает waitpid() и зомби исчезают.
 *   sigchld                   — способ избавиться №2: обработчик SIGCHLD
 *                               сразу "собирает" потомков waitpid(WNOHANG).
 *   ignore                    — способ избавиться №3: signal(SIGCHLD, SIG_IGN),
 *                               ядро само не оставляет зомби.
 *
 * Пока родитель спит, в соседнем терминале можно выполнить
 *   ps -o pid,ppid,stat,comm -p <pid'ы потомков>   или   ps aux | grep -w Z
 * а также убить родителя (kill -9 <pid родителя>) и увидеть, что зомби
 * усыновляет init (PID 1) и они исчезают (способ избавиться №4).
 */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>

#define CHILDREN 3

static pid_t children[CHILDREN];

/* Показывает состояние потомков с помощью утилиты ps.
 * В колонке STAT буква Z означает зомби. */
static void ShowChildren(const char *title) {
  char cmd[256];
  int len = snprintf(cmd, sizeof(cmd), "ps -o pid,ppid,stat,comm -p ");
  for (int i = 0; i < CHILDREN; i++)
    len += snprintf(cmd + len, sizeof(cmd) - len, "%s%d", i ? "," : "",
                    (int)children[i]);
  printf("\n--- %s ---\n$ %s\n", title, cmd);
  fflush(stdout);
  /* system() сам создаёт потомка (sh) и ждёт его, поэтому на время вызова
   * возвращаем SIGCHLD стандартное поведение, иначе system() не сможет
   * дождаться своего sh в режимах sigchld/ignore. */
  struct sigaction old_sa, dfl_sa;
  memset(&dfl_sa, 0, sizeof(dfl_sa));
  dfl_sa.sa_handler = SIG_DFL;
  sigaction(SIGCHLD, &dfl_sa, &old_sa);
  int rc = system(cmd);
  sigaction(SIGCHLD, &old_sa, NULL);
  if (rc != 0)
    printf("(ps ничего не нашёл — этих процессов больше нет в таблице процессов)\n");
}

/* Обработчик SIGCHLD: собирает ВСЕХ завершившихся потомков.
 * Цикл нужен, потому что несколько SIGCHLD могут "склеиться" в один. */
static void SigchldHandler(int signum) {
  (void)signum;
  int saved_errno = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0) {
  }
  errno = saved_errno;
}

int main(int argc, char **argv) {
  const char *mode = argc > 1 ? argv[1] : "zombie";
  int pause_sec = argc > 2 ? atoi(argv[2]) : 20;
  if (pause_sec <= 0) pause_sec = 20;

  if (strcmp(mode, "sigchld") == 0) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SigchldHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
  } else if (strcmp(mode, "ignore") == 0) {
    signal(SIGCHLD, SIG_IGN);
  } else if (strcmp(mode, "zombie") != 0) {
    printf("Usage: %s [zombie|sigchld|ignore] [seconds]\n", argv[0]);
    return 1;
  }

  printf("Режим: %s. PID родителя: %d\n", mode, (int)getpid());
  fflush(stdout); /* иначе буфер stdout скопируется в потомков при fork */

  for (int i = 0; i < CHILDREN; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      return 1;
    }
    if (pid == 0) {
      /* потомок: сообщает о себе и сразу завершается с кодом i */
      printf("  потомок #%d: PID %d, PPID %d — завершаюсь с кодом %d\n", i,
             (int)getpid(), (int)getppid(), i);
      exit(i);
    }
    children[i] = pid;
  }

  /* даём потомкам гарантированно завершиться; sleep может прерваться
   * сигналом SIGCHLD, поэтому досыпаем оставшееся время в цикле */
  unsigned int left = 1;
  while ((left = sleep(left)) > 0) {
  }
  ShowChildren("Состояние потомков через 1 с после их завершения");

  if (strcmp(mode, "zombie") == 0) {
    printf("\nПотомки мертвы, но остались в таблице процессов (STAT = Z, <defunct>):\n"
           "ядро хранит их PID и код возврата, пока родитель не вызовет wait().\n"
           "Родитель спит %d с. Проверьте в другом терминале:\n"
           "  ps aux | grep -w Z\n"
           "или убейте родителя: kill -9 %d — зомби усыновит init и они исчезнут.\n",
           pause_sec, (int)getpid());
    fflush(stdout);
    sleep(pause_sec);

    printf("\nРодитель вызывает waitpid() для каждого потомка:\n");
    for (int i = 0; i < CHILDREN; i++) {
      int status;
      pid_t pid = waitpid(children[i], &status, 0);
      if (pid > 0 && WIFEXITED(status))
        printf("  собран PID %d, код возврата %d\n", (int)pid, WEXITSTATUS(status));
    }
    ShowChildren("После waitpid()");
  } else {
    printf("\nЗомби нет: %s\n",
           strcmp(mode, "sigchld") == 0
               ? "обработчик SIGCHLD сразу вызвал waitpid() для каждого потомка."
               : "при SIGCHLD = SIG_IGN ядро сразу удаляет завершившихся потомков.");
  }
  return 0;
}
