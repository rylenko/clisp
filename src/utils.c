#include <stdlib.h>
#include <string.h>

/* Copies `s` to new allocated memory. */
char*
strdup(const char *s)
{
	char *rv = malloc(strlen(s) + 1);
	strcpy(rv, s);
	return rv;
}
