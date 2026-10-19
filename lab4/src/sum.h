#ifndef SUM_H
#define SUM_H

/* Аргументы для подсчёта суммы куска массива [begin, end) */
struct SumArgs {
  int *array;
  int begin;
  int end;
};

/* Сумма элементов array[begin..end-1].
 * Возвращает long long: сумма даже тысячи значений rand() (до 2^31-1)
 * переполнила бы int. */
long long Sum(const struct SumArgs *args);

#endif
