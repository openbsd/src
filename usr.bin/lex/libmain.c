/*	$OpenBSD: libmain.c,v 1.4 1996/12/14 07:07:56 tholo Exp $	*/

/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/libmain.c,v 1.4 1996/12/14 07:07:56 tholo Exp $ */

#include <sys/cdefs.h>

int yylex __P((void));
int main __P((int, char **, char **));

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
