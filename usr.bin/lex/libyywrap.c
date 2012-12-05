/*	$OpenBSD: libyywrap.c,v 1.7 2012/12/05 23:20:25 deraadt Exp $	*/

/* libyywrap - flex run-time support library "yywrap" function */

/* $Header: /home/cvs/src/usr.bin/lex/libyywrap.c,v 1.7 2012/12/05 23:20:25 deraadt Exp $ */

int yywrap(void);

int
yywrap(void)
{
	return 1;
}
