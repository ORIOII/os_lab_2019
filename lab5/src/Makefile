CC = gcc
# -pthread подключает библиотеку POSIX threads (и при компиляции, и при линковке).
# -O0 — без оптимизаций: так гонка в mutex.c проявляется надёжнее всего.
CFLAGS = -Wall -Wextra -O0 -pthread

.PHONY: all task1 task2 task3 tsan clean

all: task1 task2 task3

# ================= Задание 1: mutex.c без мьютекса и с мьютексом =================
task1: mutex_nolock mutex_lock

# Без мьютекса: макрос USE_MUTEX не определён, LOCK/UNLOCK ничего не делают
mutex_nolock: mutex.c
	$(CC) $(CFLAGS) -o mutex_nolock mutex.c

# С мьютексом: -DUSE_MUTEX включает pthread_mutex_lock/unlock
mutex_lock: mutex.c
	$(CC) $(CFLAGS) -DUSE_MUTEX -o mutex_lock mutex.c

# Бонус: версия без мьютекса под ThreadSanitizer — он сам находит гонку и
# печатает "WARNING: ThreadSanitizer: data race"
tsan: mutex.c
	$(CC) $(CFLAGS) -g -fsanitize=thread -o mutex_nolock_tsan mutex.c

# ================= Задание 2: факториал по модулю =================
task2: factorial

factorial: factorial.c
	$(CC) $(CFLAGS) -o factorial factorial.c

# ================= Задание 3: deadlock =================
task3: deadlock

deadlock: deadlock.c
	$(CC) $(CFLAGS) -o deadlock deadlock.c

clean:
	rm -f *.o mutex_nolock mutex_lock mutex_nolock_tsan factorial deadlock
