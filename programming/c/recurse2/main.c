#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define STRINGLEN  128
static char s0[STRINGLEN];
static char s1[STRINGLEN];

static void recurse(char *sp0, char *sp1) {
   if (*sp0 != 0) {
      recurse(sp0 + 1, sp1 - 1);
      *sp1 = *sp0;
   }
}

int main(int argc, char **argv, char **envv)
{
   strcpy(s0, "1234567890");
   memset(s1, 0, STRINGLEN);
   int n = strlen(s0);
   printf("Original: %s n=%d\n", s0, n);
   recurse(s0, s1 + (n - 1));
   printf("Reversed: %s\n", s1);
   return 0;
}

