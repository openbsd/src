/* libyywrap - flex run-time support library "yywrap" function */

/* $Header: /home/cvs/src/usr.bin/lex/libyywrap.c,v 1.1.1.1 1995/10/18 08:45:31 deraadt Exp $ */

#include <sys/cdefs.h>

int yywrap __P((void));

int yywrap()
	{
	return 1;
	}
