/*	$NetBSD: dotest.c,v 1.2 1998/01/09 08:03:54 perry Exp $	*/

#include <stdio.h>

void print_str(const char *s);
void print_num(const int);
void itest(void);
void ftest1(void);
void ftest2(void);
void ftest3(void);

void
print_str(s)
	const char *s;
{
	printf("%s", s);
	fflush(stdout);
}

void
print_num(i)
	int i;
{
	printf("%d", i);
	fflush(stdout);
}

int
main()
{
	itest();
	ftest1();
	ftest2();
#if 0
	/*
	 * We would need a special kernel, that clears the exception condition
	 * and does RTE, to run this.
	 */
	ftest3();
#endif
	exit (0);
}
