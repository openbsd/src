/*	$OpenBSD: libyywrap.c,v 1.5 2002/02/16 21:27:47 millert Exp $	*/

/* libyywrap - flex run-time support library "yywrap" function */

/* $Header: /home/cvs/src/usr.bin/lex/libyywrap.c,v 1.5 2002/02/16 21:27:47 millert Exp $ */

#include <sys/cdefs.h>

int yywrap(void);

int
yywrap()
	{
	return 1;
	}
