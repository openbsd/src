/*	$OpenBSD: fnm_test.c,v 1.1 2008/10/01 23:04:58 millert Exp $	*/

/*
 * Public domain, 2008, Todd C. Miller <Todd.Miller@courtesan.com>
 */

#include <err.h>
#include <fnmatch.h>
#include <stdio.h>
#include <stdlib.h>

int
main(int argc, char **argv)
{
	FILE *fp = stdin;
	char pattern[1024], string[1024];
	int errors = 0, flags, got, want;

	if (argc > 1) {
		if ((fp = fopen(argv[1], "r")) == NULL)
			err(1, "%s", argv[1]);
	}

	/*
	 * Read in test file, which is formatted thusly:
	 *
	 * pattern string flags expected_result
	 *
	 */
	for (;;) {
		got = fscanf(fp, "%s %s 0x%x %d\n", pattern, string, &flags,
		    &want);
		if (got == EOF)
			break;
		if (got == 4) {
			got = fnmatch(pattern, string, flags);
			if (got != want) {
				warnx("%s %s %d: want %d, got %d", pattern,
				    string, flags, want, got);
				errors++;
			}
		}
	}
	exit(errors);
}
