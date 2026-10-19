/*
 * Лабораторная работа №4, задание 5.
 *
 *   ./psum --threads_num "num" --seed "num" --array_size "num"
 *
 * Массив генерируется функцией GenerateArray из ЛР3 (utils.c), затем
 * делится на threads_num кусков; каждый кусок суммирует свой поток
 * (POSIX threads). Функция Sum вынесена в библиотеку libsum (sum.h/sum.c).
 * В замер времени попадает только параллельный подсчёт суммы
 * (создание потоков, вычисление и pthread_join), генерация массива — нет.
 */
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>

#include "sum.h"
#include "utils.h"

/* Функция потока. pthread_create умеет запускать только функции вида
 * void *f(void *), поэтому аргументы передаём указателем на структуру,
 * а результат возвращаем указателем на выделенную в куче память. */
void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  long long *result = malloc(sizeof(long long));
  if (result != NULL) *result = Sum(sum_args);
  return result; /* этот указатель получит pthread_join */
}

static double NowMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char **argv) {
  int threads_num = -1;
  int array_size = -1;
  int seed = -1;

  while (true) {
    static struct option options[] = {{"threads_num", required_argument, 0, 0},
                                      {"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {0, 0, 0, 0}};
    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);
    if (c == -1) break;

    if (c == 0) {
      int value = atoi(optarg);
      if (value <= 0) {
        printf("%s must be a positive number\n", options[option_index].name);
        return 1;
      }
      switch (option_index) {
        case 0: threads_num = value; break;
        case 1: seed = value; break;
        case 2: array_size = value; break;
      }
    } else if (c == '?') {
      return 1; /* getopt уже напечатал сообщение о неизвестной опции */
    }
  }

  if (optind < argc || threads_num == -1 || seed == -1 || array_size == -1) {
    printf("Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n",
           argv[0]);
    return 1;
  }
  if (threads_num > array_size) {
    printf("threads_num should not be greater than array_size\n");
    return 1;
  }

  /* Генерация массива — до начала замера времени */
  int *array = malloc(sizeof(int) * array_size);
  if (array == NULL) {
    printf("Error: not enough memory for array\n");
    return 1;
  }
  GenerateArray(array, array_size, seed);

  pthread_t threads[threads_num];
  struct SumArgs args[threads_num];

  /* Разбиение: каждому потоку chunk элементов, последнему — остаток */
  int chunk = array_size / threads_num;
  for (int i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = i * chunk;
    args[i].end = (i == threads_num - 1) ? array_size : (i + 1) * chunk;
  }

  double start = NowMs();

  for (int i = 0; i < threads_num; i++) {
    /* каждому потоку — СВОЙ элемент args[i] */
    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  long long total_sum = 0;
  for (int i = 0; i < threads_num; i++) {
    void *ret = NULL;
    pthread_join(threads[i], &ret); /* ждём завершения i-го потока */
    if (ret == NULL) {
      printf("Error: thread %d failed\n", i);
      return 1;
    }
    total_sum += *(long long *)ret;
    free(ret);
  }

  double elapsed = NowMs() - start;

  free(array);
  printf("Total: %lld\n", total_sum);
  printf("Elapsed time: %f ms\n", elapsed);
  return 0;
}
