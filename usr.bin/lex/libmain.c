/*	$OpenBSD: libmain.c,v 1.6 2003/07/28 20:38:31 deraadt Exp $	*/

/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/libmain.c,v 1.6 2003/07/28 20:38:31 deraadt Exp $ */

#include <sys/cdefs.h>

int yylex(void);
int main(int, char **);

/* ARGSUSED */
int
main(int argc, char *argv[])
{
	while (yylex() != 0)
		;

	return 0;
}
