/* Program to display address information about the process */
/* Adapted from Gray, J., program 1.4 */
/*
 * Лабораторная работа №4, задание 3.
 *
 * Исправления относительно исходного файла (иначе современные gcc/clang
 * выдают ошибки или неверные адреса):
 *   - main() и showit() объявлены с типами (неявный int запрещён с C99);
 *   - адреса печатаются через %p, а не %8X: на 64-битной системе адрес
 *     занимает 8 байт и %X обрезал бы его до младших 4 байт;
 *   - etext/edata/end объявлены как char (важен только их АДРЕС, значения
 *     по этим адресам смысла не имеют);
 *   - на macOS у линковщика нет символов etext/edata/end, вместо них
 *     используются функции из <mach-o/getsect.h> (см. ниже).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* Below is a macro definition */
#define SHW_ADR(ID, I) (printf("ID %s \t is at virtual address: %p\n", ID, (void *)&I))

#ifdef __APPLE__
#include <mach-o/getsect.h>
#include <mach-o/ldsyms.h>
/* macOS: границы сегментов берём из заголовка Mach-O исполняемого файла
 * (с учётом ASLR-сдвига). Смысл тот же, что у etext/edata/end в Linux. */
static char *addr_etext(void) {
  unsigned long size = 0;
  uint8_t *p = getsegmentdata(&_mh_execute_header, "__TEXT", &size);
  return (char *)p + size;
}
static char *addr_edata(void) {
  unsigned long size = 0;
  uint8_t *p = getsectiondata(&_mh_execute_header, "__DATA", "__data", &size);
  return (char *)p + size;
}
static char *addr_end(void) {
  unsigned long size = 0;
  uint8_t *p = getsegmentdata(&_mh_execute_header, "__DATA", &size);
  return (char *)p + size;
}
#else
/* Linux: эти символы создаёт линковщик (ld), см. man 3 end */
extern char etext, edata, end; /* Global variables for process memory */
#define addr_etext() ((void *)&etext)
#define addr_edata() ((void *)&edata)
#define addr_end() ((void *)&end)
#endif

char *cptr = "This message is output by the function showit()\n"; /* Static */
char buffer1[25];
int showit(char *p); /* Function prototype */

int main(void) {
  int i = 0; /* Automatic variable */

  /* Printing addressing information */
  printf("\nAddress etext: %p \n", (void *)addr_etext());
  printf("Address edata: %p \n", (void *)addr_edata());
  printf("Address end  : %p \n", (void *)addr_end());

  SHW_ADR("main", main);
  SHW_ADR("showit", showit);
  SHW_ADR("cptr", cptr);
  SHW_ADR("buffer1", buffer1);
  SHW_ADR("i", i);
  strcpy(buffer1, "A demonstration\n");   /* Library function */
  fflush(stdout); /* сбросить буфер printf, чтобы вывод шёл по порядку */
  write(1, buffer1, strlen(buffer1) + 1); /* System call */
  showit(cptr);

  return 0;
} /* end of main function */

/* A function follows */
int showit(char *p) {
  char *buffer2;
  SHW_ADR("buffer2", buffer2);
  if ((buffer2 = (char *)malloc((unsigned)(strlen(p) + 1))) != NULL) {
    printf("Alocated memory at %p\n", (void *)buffer2);
    strcpy(buffer2, p);    /* copy the string */
    printf("%s", buffer2); /* Didplay the string */
    free(buffer2);         /* Release location */
  } else {
    printf("Allocation error\n");
    exit(1);
  }
  return 0;
}
