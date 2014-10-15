#include <stdio.h>
#include <string.h>

static char s0[128];
static char s1[128];

static void recurse(char *sp0, char *sp1);

int main(void)
{
	strcpy(s0, "12345678");
	int n = strlen(s0);
	printf("Original = %s\n", s0);
	recurse(s0, s1 + (n - 1));
	printf("Reversed = %s\n", s1);

	return 0;
}

void recurse(char *sp0, char *sp1)
{
	if (*sp0 == 0)
		return;

	recurse(sp0 + 1, sp1 - 1);
	*sp1 = *sp0;
}
