/*	$OpenBSD: libyywrap.c,v 1.2 1996/06/26 05:35:37 deraadt Exp $	*/

/* libyywrap - flex run-time support library "yywrap" function */

/* $Header: /home/cvs/src/usr.bin/lex/libyywrap.c,v 1.2 1996/06/26 05:35:37 deraadt Exp $ */

#include <sys/cdefs.h>

int yywrap __P((void));

int yywrap()
	{
	return 1;
	}
