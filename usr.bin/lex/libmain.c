/*	$OpenBSD: libmain.c,v 1.5 2002/02/16 21:27:47 millert Exp $	*/

/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/libmain.c,v 1.5 2002/02/16 21:27:47 millert Exp $ */

#include <sys/cdefs.h>

int yylex(void);
int main(int, char **, char **);

/* ARGSUSED */
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
