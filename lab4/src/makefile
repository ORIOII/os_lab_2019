CC = gcc
LAB3 = ../../lab3/src
CFLAGS = -I. -I$(LAB3) -Wall

.PHONY: all task4 task6 clean

all: task4 task6 zombie

# ================= Задание 4: программы из заданий 1 и 3 =================
task4: parallel_min_max process_memory

# Задание 1. GenerateArray и GetMinMax берём из ЛР3
parallel_min_max: parallel_min_max.c utils.o find_min_max.o
	$(CC) $(CFLAGS) -o parallel_min_max parallel_min_max.c utils.o find_min_max.o

# Та же программа, но потомок #i перед вычислением спит 2*i секунд —
# чтобы продемонстрировать срабатывание --timeout
parallel_min_max_slow: parallel_min_max.c utils.o find_min_max.o
	$(CC) $(CFLAGS) -DCHILD_DELAY=2 -o parallel_min_max_slow parallel_min_max.c utils.o find_min_max.o

# Задание 3
process_memory: process_memory.c
	$(CC) $(CFLAGS) -o process_memory process_memory.c

utils.o: $(LAB3)/utils.c $(LAB3)/utils.h
	$(CC) $(CFLAGS) -c $(LAB3)/utils.c -o utils.o

find_min_max.o: $(LAB3)/find_min_max.c $(LAB3)/find_min_max.h $(LAB3)/utils.h
	$(CC) $(CFLAGS) -c $(LAB3)/find_min_max.c -o find_min_max.o

# ================= Задание 2: демонстрация зомби =================
zombie: zombie.c
	$(CC) $(CFLAGS) -o zombie zombie.c

# ================= Задание 6: parallel_sum =================
task6: psum

# Программа: parallel_sum.c + статическая библиотека libsum.a + GenerateArray из ЛР3.
# -pthread подключает библиотеку POSIX threads (и при компиляции, и при линковке)
psum: parallel_sum.c libsum.a utils.o sum.h
	$(CC) $(CFLAGS) -pthread -o psum parallel_sum.c utils.o -L. -lsum

# Статическая библиотека с функцией Sum
libsum.a: sum.o
	ar rcs libsum.a sum.o

sum.o: sum.c sum.h
	$(CC) $(CFLAGS) -c sum.c -o sum.o

clean:
	rm -f *.o *.a parallel_min_max parallel_min_max_slow process_memory zombie psum min_max_*.txt
