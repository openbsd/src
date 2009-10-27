/*	$OpenBSD: percent_m.c,v 1.4 2009/10/27 23:59:30 deraadt Exp $	*/

 /*
  * Replace %m by system error message.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#ifndef SYS_ERRLIST_DEFINED
extern char *sys_errlist[];
extern int sys_nerr;
#endif

char   *percent_m(obuf, ibuf)
char   *obuf;
char   *ibuf;
{
    char   *bp = obuf;
    char   *cp = ibuf;
    int len = BUFSIZ;

    while ((*bp = *cp)) {
	if (*cp == '%' && cp[1] == 'm') {
	    len = BUFSIZ - (bp - obuf);
	    if (errno < sys_nerr && errno > 0) {
		strlcpy(bp, sys_errlist[errno], len);
	    } else {
		snprintf(bp, len, "Unknown error %d", errno);
	    }
	    bp += strlen(bp);
	    cp += 2;
	} else {
	    bp++, cp++;
	}
    }
    return (obuf);
}
