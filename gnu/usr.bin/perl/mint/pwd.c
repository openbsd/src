/* pwd.c - replacement for broken pwd command.
 * Copyright 1997 Guido Flohr, <gufl0000@stud.uni-sb.de>.
 * Do with it as you please.
 */
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#if defined(__STDC__) || defined(__cplusplus)
int main (int argc, char* argv[])
#else
int main (argc, argv)
	int argc;
	char* argv[];
#endif
{
	char path_buf[PATH_MAX + 1];
	
	if (argc > 1) {
		int i;
		
		fflush (stdout);
		fputs (argv[0], stderr);
		fputs (": ignoring garbage arguments\n", stderr);
	}
	
	if (!getcwd (path_buf, PATH_MAX + 1)) {
		fflush (stdout);
		/* Save space, memory and the whales, avoid fprintf.  */
		fputs (argv[0], stderr);
		fputs (": can\'t get current working directory: ", stderr);
		fputs (strerror (errno), stderr);
		fputc ('\n', stderr);
		return 1;
	}
	if (puts (path_buf) < 0) {
		return 1;
	}
	return 0;
}
/* End of pwd.c.  */
