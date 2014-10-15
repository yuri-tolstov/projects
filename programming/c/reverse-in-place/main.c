#include <stdio.h>
#include <string.h>

static char s0[128];
static char *sp;

static void recurse(char *p);

int main(void)
{
	strcpy(s0, "12345678");
	sp = s0;
	printf("Original = %s\n", s0);
	recurse(s0);
	printf("Reversed = %s\n", s0);

	return 0;
}

void recurse(char *p)
{
	char c = *p;

	if (c == 0)
		return;

	recurse(p + 1);
	*sp++ = c;
}
