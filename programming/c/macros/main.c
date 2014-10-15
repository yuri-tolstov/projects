#include <stdio.h>
#include <string.h>

#define MYDEF_1  1
// #define MYDEF_2  2

int main(void)
{
   int i = 3;

#if !(defined(MYDEF_1) || defined(MYDEF_2))
   printf("MYDEF: i=%d\n", i);
#else
   printf("NO MYDEF: i=%d\n", i);
#endif
   return 0;
}

