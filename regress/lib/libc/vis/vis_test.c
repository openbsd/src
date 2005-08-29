/*	$OpenBSD */

/* Public domain. 2005, Otto Moerbeek */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <vis.h>

int
main()
{
	char inp[UCHAR_MAX + 1];
	char out[4 * UCHAR_MAX + 1];
	int i;

	for (i = 0; i <= UCHAR_MAX; i++) {
		inp[i] = i;
	}
	strvisx(out, inp, UCHAR_MAX + 1, 0);
	printf("%s\n", out);
	exit(0);
}
