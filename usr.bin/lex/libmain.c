/*	$OpenBSD: libmain.c,v 1.3 1996/07/13 22:22:05 millert Exp $	*/

/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/libmain.c,v 1.3 1996/07/13 22:22:05 millert Exp $ */

#include <sys/cdefs.h>

int yylex __P((void));
int main __P((int, char **, char **));

int
main( argc, argv, envp )
int argc;
char *argv[];
char *envp[];
	{
	while ( yylex() != 0 )
		;

	return 0;
	}
