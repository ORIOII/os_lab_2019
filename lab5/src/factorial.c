/*
 * Лабораторная работа №5, задание 2.
 *
 * Параллельное вычисление факториала по модулю:  k! mod m
 *
 *   ./factorial -k 10 --pnum=4 --mod=10
 *
 *   -k, --k      число, факториал которого вычисляется (k >= 0)
 *   --pnum       количество потоков (pnum >= 1)
 *   --mod        модуль (mod >= 1)
 *   -v, --verbose  печатать, какой диапазон считает каждый поток
 *
 * Идея: числа 1..k делятся на pnum диапазонов. Каждый поток перемножает (по
 * модулю mod) числа своего диапазона и получает частичное произведение.
 * Затем поток умножает общий результат на своё частичное произведение.
 * Общий результат — разделяемая переменная, поэтому это умножение
 * выполняется внутри критической секции, защищённой мьютексом.
 *
 * Корректность: (a * b) mod m = ((a mod m) * (b mod m)) mod m, а умножение
 * коммутативно и ассоциативно, поэтому порядок, в котором потоки
 * «вливают» свои произведения в общий результат, не важен.
 */
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

typedef unsigned long long u64;

/* Что должен посчитать один поток: произведение чисел begin..end включительно
 * по модулю mod. Если begin > end, диапазон пуст и произведение равно 1. */
struct FactorialArgs {
  int id;
  u64 begin;
  u64 end;
  u64 mod;
  bool verbose;
};

/* Разделяемые данные: общий результат и мьютекс, который его охраняет. */
static u64 result;
static pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;

/* (a * b) mod m без переполнения: промежуточное произведение двух 64-битных
 * чисел хранится в 128-битном типе (расширение gcc/clang). */
static u64 MulMod(u64 a, u64 b, u64 m) {
  return (u64)(((unsigned __int128)a * b) % m);
}

void *ThreadFactorial(void *arg) {
  struct FactorialArgs *a = (struct FactorialArgs *)arg;

  /* Частичное произведение считаем локально — без блокировок, ведь это
   * личные данные потока. 1 % mod, а не просто 1: при mod == 1 любое
   * число по модулю даёт 0. */
  u64 partial = 1 % a->mod;
  for (u64 i = a->begin; i <= a->end; i++) {
    partial = MulMod(partial, i % a->mod, a->mod);
  }

  if (a->verbose) {
    if (a->begin <= a->end)
      printf("thread %d: %llu..%llu -> partial = %llu\n", a->id, a->begin,
             a->end, partial);
    else
      printf("thread %d: (empty range) -> partial = %llu\n", a->id, partial);
  }

  /* Критическая секция: только обновление общего результата. */
  pthread_mutex_lock(&result_mutex);
  result = MulMod(result, partial, a->mod);
  pthread_mutex_unlock(&result_mutex);

  return NULL;
}

/* Разбор неотрицательного целого. Отвергает пустую строку, минус, мусор в
 * конце и переполнение (strtoull молча превращает "-1" в огромное число). */
static bool ParseU64(const char *s, u64 *out) {
  if (s == NULL || *s == '\0' || *s == '-') return false;
  char *end = NULL;
  errno = 0;
  u64 v = strtoull(s, &end, 10);
  if (errno != 0 || *end != '\0') return false;
  *out = v;
  return true;
}

static void Usage(const char *prog) {
  printf("Usage: %s -k <num> --pnum=<num> --mod=<num> [-v]\n", prog);
}

int main(int argc, char **argv) {
  u64 k = 0, pnum = 0, mod = 0;
  bool has_k = false, has_pnum = false, has_mod = false, verbose = false;

  static struct option options[] = {{"k", required_argument, 0, 'k'},
                                    {"pnum", required_argument, 0, 'p'},
                                    {"mod", required_argument, 0, 'm'},
                                    {"verbose", no_argument, 0, 'v'},
                                    {0, 0, 0, 0}};

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "k:v", options, &option_index);
    if (c == -1) break;

    switch (c) {
      case 'k':
        if (!ParseU64(optarg, &k)) {
          printf("k must be a non-negative integer\n");
          return 1;
        }
        has_k = true;
        break;
      case 'p':
        if (!ParseU64(optarg, &pnum) || pnum == 0) {
          printf("pnum must be a positive integer\n");
          return 1;
        }
        has_pnum = true;
        break;
      case 'm':
        if (!ParseU64(optarg, &mod) || mod == 0) {
          printf("mod must be a positive integer\n");
          return 1;
        }
        has_mod = true;
        break;
      case 'v':
        verbose = true;
        break;
      case '?':
        return 1; /* getopt уже напечатал сообщение о неизвестной опции */
    }
  }

  if (optind < argc || !has_k || !has_pnum || !has_mod) {
    Usage(argv[0]);
    return 1;
  }

  pthread_t *threads = malloc(sizeof(pthread_t) * pnum);
  struct FactorialArgs *args = malloc(sizeof(struct FactorialArgs) * pnum);
  if (threads == NULL || args == NULL) {
    printf("Error: not enough memory for %llu threads\n", pnum);
    return 1;
  }

  result = 1 % mod; /* нейтральный элемент умножения (с учётом mod == 1) */

  /* Делим числа 1..k на pnum диапазонов. Каждому потоку base чисел, а первым
   * rem потокам — ещё по одному, чтобы нагрузка отличалась не более чем на 1.
   * Если pnum > k, часть потоков получит пустой диапазон. */
  u64 base = k / pnum;
  u64 rem = k % pnum;
  u64 next = 1; /* первое ещё не розданное число */

  for (u64 i = 0; i < pnum; i++) {
    u64 count = base + (i < rem ? 1 : 0);
    args[i].id = (int)i;
    args[i].begin = next;
    args[i].end = next + count - 1; /* при count == 0 получится begin > end */
    args[i].mod = mod;
    args[i].verbose = verbose;
    next += count;
  }

  u64 started = 0;
  for (u64 i = 0; i < pnum; i++) {
    /* каждому потоку — СВОЙ элемент args[i] */
    if (pthread_create(&threads[i], NULL, ThreadFactorial, &args[i]) != 0) {
      printf("Error: pthread_create failed for thread %llu\n", i);
      break;
    }
    started++;
  }

  for (u64 i = 0; i < started; i++) {
    pthread_join(threads[i], NULL); /* ждём завершения всех запущенных */
  }

  if (started != pnum) {
    free(threads);
    free(args);
    return 1; /* результат был бы неполным — не печатаем его */
  }

  printf("%llu! mod %llu = %llu\n", k, mod, result);

  free(threads);
  free(args);
  return 0;
}
