#include <stdio.h>
#include <string.h>
#include <stdint.h>

int main(int argc, char **argv, char **envv)
{
   int a = 0x112233;
   int b = 0x445566;
   a = a ^ b;
   b = a ^ b;
   a = a ^ b;
   printf("a = %x, b = %x\n", a, b);
   return 0;
}

