/*	$OpenBSD: percent_m.c,v 1.1 1997/02/26 03:06:54 downsj Exp $	*/

 /*
  * Replace %m by system error message.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsid[] = "@(#) percent_m.c 1.1 94/12/28 17:42:37";
#else
static char rcsid[] = "$OpenBSD: percent_m.c,v 1.1 1997/02/26 03:06:54 downsj Exp $";
#endif
#endif

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

extern int errno;
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

    while ((*bp = *cp)) {
	if (*cp == '%' && cp[1] == 'm') {
	    if (errno < sys_nerr && errno > 0) {
		strcpy(bp, sys_errlist[errno]);
	    } else {
		sprintf(bp, "Unknown error %d", errno);
	    }
	    bp += strlen(bp);
	    cp += 2;
	} else {
	    bp++, cp++;
	}
    }
    return (obuf);
}
