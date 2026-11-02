/*
 * Лабораторная работа №5, задание 3.
 *
 * Демонстрация состояния deadlock (взаимной блокировки).
 *
 *   ./deadlock                 — воспроизводит deadlock
 *   ./deadlock --fix           — тот же сценарий, но с единым порядком
 *                                захвата мьютексов: deadlock'а нет
 *   ./deadlock --timeout N     — через N секунд ожидания сообщить о deadlock
 *                                и завершить программу (по умолчанию 5);
 *                                --timeout 0 — ждать вечно (программа
 *                                зависнет, останавливать через Ctrl+C)
 *
 * Сценарий. Есть два ресурса, каждый защищён своим мьютексом: A и B.
 *
 *   поток 1:  lock(A);  sleep;  lock(B);  ...  unlock(B);  unlock(A);
 *   поток 2:  lock(B);  sleep;  lock(A);  ...  unlock(A);  unlock(B);
 *
 * sleep(1) между захватами гарантирует, что к моменту второго lock оба потока
 * уже держат по одному мьютексу. Поток 1 ждёт B (его держит поток 2), а
 * поток 2 ждёт A (его держит поток 1). Каждый ждёт другого — оба не
 * продвинутся никогда.
 */
#include <getopt.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t mutex_a = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mutex_b = PTHREAD_MUTEX_INITIALIZER;

static bool fix = false;          /* единый порядок захвата? */
static atomic_int finished = 0;   /* сколько потоков дошло до конца */

void *Thread1(void *arg) {
  (void)arg;
  printf("[thread 1] locking A...\n");
  pthread_mutex_lock(&mutex_a);
  printf("[thread 1] got A, working, then wants B\n");
  sleep(1);

  printf("[thread 1] locking B...\n");
  pthread_mutex_lock(&mutex_b); /* здесь поток 1 застревает при deadlock */
  printf("[thread 1] got B -> both resources acquired\n");

  pthread_mutex_unlock(&mutex_b);
  pthread_mutex_unlock(&mutex_a);
  printf("[thread 1] done\n");
  atomic_fetch_add(&finished, 1);
  return NULL;
}

void *Thread2(void *arg) {
  (void)arg;
  /* Ошибка (deadlock): порядок B -> A, обратный потоку 1.
   * Исправление (--fix): тот же порядок A -> B, что и у потока 1. */
  pthread_mutex_t *first = fix ? &mutex_a : &mutex_b;
  pthread_mutex_t *second = fix ? &mutex_b : &mutex_a;
  const char *first_name = fix ? "A" : "B";
  const char *second_name = fix ? "B" : "A";

  printf("[thread 2] locking %s...\n", first_name);
  pthread_mutex_lock(first);
  printf("[thread 2] got %s, working, then wants %s\n", first_name,
         second_name);
  sleep(1);

  printf("[thread 2] locking %s...\n", second_name);
  pthread_mutex_lock(second); /* здесь поток 2 застревает при deadlock */
  printf("[thread 2] got %s -> both resources acquired\n", second_name);

  pthread_mutex_unlock(second);
  pthread_mutex_unlock(first);
  printf("[thread 2] done\n");
  atomic_fetch_add(&finished, 1);
  return NULL;
}

int main(int argc, char **argv) {
  int timeout = 5;

  static struct option options[] = {{"fix", no_argument, 0, 'f'},
                                    {"timeout", required_argument, 0, 't'},
                                    {0, 0, 0, 0}};
  while (true) {
    int c = getopt_long(argc, argv, "", options, NULL);
    if (c == -1) break;
    if (c == 'f') {
      fix = true;
    } else if (c == 't') {
      timeout = atoi(optarg);
      if (timeout < 0) {
        printf("timeout must be a non-negative number\n");
        return 1;
      }
    } else {
      return 1;
    }
  }
  if (optind < argc) {
    printf("Usage: %s [--fix] [--timeout N]\n", argv[0]);
    return 1;
  }

  /* При перенаправлении вывода (в файл, в конвейер) stdout буферизуется
   * блоками: отключаем буфер, чтобы сообщения не терялись и выводились сразу. */
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("Mode: %s\n", fix ? "FIXED (same lock order A -> B)"
                           : "DEADLOCK (thread 1: A -> B, thread 2: B -> A)");

  pthread_t t1, t2;
  if (pthread_create(&t1, NULL, Thread1, NULL) != 0 ||
      pthread_create(&t2, NULL, Thread2, NULL) != 0) {
    printf("Error: pthread_create failed\n");
    return 1;
  }

  if (timeout == 0) {
    /* Ждём без ограничения: при deadlock pthread_join не вернётся никогда. */
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
  } else {
    /* Проверяем раз в 100 мс, не закончились ли оба потока. */
    for (int waited_ms = 0; waited_ms < timeout * 1000; waited_ms += 100) {
      if (atomic_load(&finished) == 2) break;
      usleep(100 * 1000);
    }
    if (atomic_load(&finished) < 2) {
      printf(
          "\nDEADLOCK: after %d s only %d of 2 threads finished.\n"
          "Thread 1 holds A and waits for B; thread 2 holds B and waits for "
          "A.\n"
          "Neither can proceed - terminating the program.\n",
          timeout, atomic_load(&finished));
      return 2;
    }
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
  }

  printf("OK: both threads finished, no deadlock.\n");
  return 0;
}
