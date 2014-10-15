#include <stdio.h>
#include <string.h>

enum {
   MM_ZERO,
   MM_ONE,
   MM_TWO,
   MM_THREE,
   MM_FOUR,
   MM_MAX
};

static int aa[] = {
   [MM_ZERO] = 0,
   [MM_TWO] = 2,
   [MM_ONE] = 1,
};

int main(void)
{
   int i;

   for (i = 0; i < MM_MAX; i++) {
      printf("%d: %d\n", i, aa[i]);
   }
   return 0;
}

