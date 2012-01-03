/* 4.4BSD standard API replacement for strtok()
 *
 * The strsep() function locates, in the string referenced by *stringp, the
 * first occurrence of any character in the string delim (or the terminating
 * `\0' character) and replaces it with a `\0'.  The location of the next
 * character after the delimiter character (or NULL, if the end of the
 * string was reached) is stored in *stringp.  The original value of
 * *stringp is returned.
 * 
 * An ``empty'' field (i.e., a character in the string delim occurs as the
 * first character of *stringp) can be detected by comparing the location
 * referenced by the returned pointer to `\0'.
 *
 * If *stringp is initially NULL, strsep() returns NULL.
 */

#include "strsep.h"

#include <string.h>

char*
strsep_s (
	char**		stringp,
	const char*	delim,
	size_t		numberOfElements
	)
{
	char* start = *stringp;
	char* ptr;

	if (!start)
		return NULL;

	if (!*delim)
		ptr = start + strnlen_s (start, numberOfElements);
	else {
		ptr = strpbrk (start, delim);
		if (!ptr) {
			*stringp = NULL;
			return start;
		}
	}

	*ptr = '\0';
	*stringp = ptr + 1;

	return start;
}

/* eof */