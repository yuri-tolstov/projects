#include <stdio.h>
#include <string.h>
#include <stdint.h>

static char ss[128];
static char *sp;

static void recurse(char *p) {
   char c = *p;
   if (*p != 0) {
      recurse(p + 1);
      *sp++ = c;
   }
}

int main(int argc, char **argv, char **envv)
{
   strcpy(ss, "1234567890");
   sp = ss;
   printf("Original: %s\n", ss);
   recurse(ss);
   printf("Reversed: %s\n", ss);
   return 0;
}

