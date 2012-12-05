/*	$OpenBSD: libmain.c,v 1.7 2012/12/05 23:20:25 deraadt Exp $	*/

/* libmain - flex run-time support library "main" function */

/* $Header: /home/cvs/src/usr.bin/lex/libmain.c,v 1.7 2012/12/05 23:20:25 deraadt Exp $ */


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
