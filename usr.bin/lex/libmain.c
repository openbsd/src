/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/libmain.c,v 1.1.1.1 1995/10/18 08:45:31 deraadt Exp $ */

#include <sys/cdefs.h>

int yylex __P((void));
int main __P((int, char **, char **));

int
main( argc, argv, envp )
int argc;
char *argv[];
char *envp[];
	{
	return yylex();
	}
