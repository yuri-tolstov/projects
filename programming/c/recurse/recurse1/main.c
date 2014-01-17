#include <stdio.h>
#include <string.h>
#include <stdint.h>

static char s[128];
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
   strcpy(s, "1234567890");
   sp = s;
   printf("Original: %s\n", s);
   recurse(s);
   printf("Reversed: %s\n", s);
   return 0;
}

