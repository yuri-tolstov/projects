#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char **argv, char **envv)
{
   int a = 0x112233;
   int b = 0x445566;
   printf("Original: a = %x, b = %x\n", a, b);
#if 0
   a = a ^ b;
   b = a ^ b;
   a = a ^ b;
#else
   a = a + b;
   b = a - b;
   a = a - b;
#endif
   printf("Swapped: a = %x, b = %x\n", a, b);
   return 0;
}

