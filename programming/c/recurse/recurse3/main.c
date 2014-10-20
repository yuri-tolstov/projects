#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define STRINGLEN  128

static void recurse(char *sp0, char *sp1) {
   if (sp0 <= sp1) {
      recurse(sp0 + 1, sp1 - 1);
      *sp0 = *sp0 ^ *sp1;
      *sp1 = *sp0 ^ *sp1;
      *sp0 = *sp0 ^ *sp1;
   }
}

int main(int argc, char **argv, char **envv)
{
   int n;
   char *mb, *sp;

   /* Setup string. */
   mb = (char *)malloc(STRINGLEN);
   memset(mb, 0, STRINGLEN);
   strcpy(mb, "1234567890");
   /* String length. */
   for (sp = mb, n = 0; *sp != 0; sp++, n++);
   printf("Original: %s n=%d\n", mb, n);
   /* Swap in-place. */
   recurse(mb, mb + (n - 1));
   printf("Reversed: %s\n", mb);
   /* Cleanup. */
   free(mb);
   return 0;
}

