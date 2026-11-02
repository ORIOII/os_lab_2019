/********************************************************
 * An example source module to accompany...
 *
 * "Using POSIX Threads: Programming with Pthreads"
 *     by Brad nichols, Dick Buttlar, Jackie Farrell
 *     O'Reilly & Associates, Inc.
 *  Modified by A.Kostin
 ********************************************************
 * mutex.c
 *
 * Simple multi-threaded example with a mutex lock.
 *
 * Лабораторная работа №5, задание 1.
 *
 * Одна и та же программа собирается в ДВЕ версии (см. makefile):
 *
 *   mutex_nolock  — без мьютекса (макрос USE_MUTEX не определён);
 *   mutex_lock    — с мьютексом (gcc -DUSE_MUTEX ...).
 *
 * Два потока по 50 раз увеличивают общую переменную common на 1.
 * Правильный итог — 2 * 50 = 100. Увеличение сделано намеренно «медленным»
 * (прочитать -> долгий цикл -> записать), чтобы состояние гонки проявлялось
 * почти всегда.
 */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define ITERATIONS 50 /* сколько раз каждый поток увеличивает счётчик */
#define SPIN 500000UL /* длина «долгого» цикла внутри критической секции */

#ifdef USE_MUTEX
#define LOCK(m) pthread_mutex_lock(m)
#define UNLOCK(m) pthread_mutex_unlock(m)
#else
#define LOCK(m) ((void)0) /* без мьютекса защиты нет */
#define UNLOCK(m) ((void)0)
#endif

void *do_one_thing(void *);
void *do_another_thing(void *);
void do_wrap_up(int);

int common = 0; /* A shared variable for two threads */
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

int main() {
  pthread_t thread1, thread2;

#ifdef USE_MUTEX
  printf("Mode: WITH mutex\n");
#else
  printf("Mode: WITHOUT mutex\n");
#endif

  if (pthread_create(&thread1, NULL, do_one_thing, (void *)&common) != 0) {
    perror("pthread_create");
    exit(1);
  }

  if (pthread_create(&thread2, NULL, do_another_thing, (void *)&common) != 0) {
    perror("pthread_create");
    exit(1);
  }

  if (pthread_join(thread1, NULL) != 0) {
    perror("pthread_join");
    exit(1);
  }

  if (pthread_join(thread2, NULL) != 0) {
    perror("pthread_join");
    exit(1);
  }

  do_wrap_up(common);

  return 0;
}

/* Функции потоков имеют вид void *f(void *), как требует pthread_create. */
void *do_one_thing(void *arg) {
  int *pnum_times = (int *)arg;
  int i;
  volatile unsigned long k; /* volatile — чтобы компилятор не выбросил цикл */
  int work;
  for (i = 0; i < ITERATIONS; i++) {
    LOCK(&mut); /* ---- начало критической секции ---- */
    printf("doing one thing\n");
    work = *pnum_times;
    printf("counter = %d\n", work);
    work++; /* increment, but not write */
    for (k = 0; k < SPIN; k++)
      ;                 /* long cycle */
    *pnum_times = work; /* write back */
    UNLOCK(&mut);       /* ---- конец критической секции ---- */
  }
  return NULL;
}

void *do_another_thing(void *arg) {
  int *pnum_times = (int *)arg;
  int i;
  volatile unsigned long k;
  int work;
  for (i = 0; i < ITERATIONS; i++) {
    LOCK(&mut);
    printf("doing another thing\n");
    work = *pnum_times;
    printf("counter = %d\n", work);
    work++; /* increment, but not write */
    for (k = 0; k < SPIN; k++)
      ;                 /* long cycle */
    *pnum_times = work; /* write back */
    UNLOCK(&mut);
  }
  return NULL;
}

void do_wrap_up(int counter) {
  printf("All done, counter = %d (expected %d)\n", counter, 2 * ITERATIONS);
}
