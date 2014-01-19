#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

typedef struct mytest_s {
   int a;
   short b;
   char c;
} mytest_t;

int main(int argc, char **argv, char **envv)
{
   mytest_t to;
   long ap = ALIGN(sizeof(to), 32);
   
   printf("oo = %lx, ao = %lx\n", (intptr_t)&to, (intptr_t)((void *)&to + ap));
   return 0;
}

