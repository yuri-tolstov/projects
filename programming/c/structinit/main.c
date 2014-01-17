#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct mystruct_s {
   int a;
   int b;
   int c;
} mystruct_t;


static mystruct_t mys[] = {
   [0] = {
      .a = 0,
      .b = 0,
   },
   [1] = {
      .a = 1,
      .b = 1,
   },
   [2] = {
      .a = 2,
      .b = 2,
   }
};

int main(int argc, char **argv, char **envv)
{
   int i;

   for (i = 0; i < 2; i++) {
      printf("%d: a=%d b=%d c=%d\n", i, mys[i].a, mys[i].b, mys[i].c);
   }
   return 0;
}

