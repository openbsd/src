/*	$OpenBSD: misc.c,v 1.2 1997/06/30 06:06:00 deraadt Exp $	*/

 /*
  * Misc routines that are used by tcpd and by tcpdchk.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#ifndef lint
#if 0
static char sccsic[] = "@(#) misc.c 1.2 96/02/11 17:01:29";
#else
static char rcsid[] = "$OpenBSD: misc.c,v 1.2 1997/06/30 06:06:00 deraadt Exp $";
#endif
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "tcpd.h"

#ifndef	INADDR_NONE
#define	INADDR_NONE	(-1)		/* XXX should be 0xffffffff */
#endif

/* xgets - fgets() with backslash-newline stripping */

char   *xgets(ptr, len, fp)
char   *ptr;
int     len;
FILE   *fp;
{
    int     got;
    char   *start = ptr;

    while (fgets(ptr, len, fp)) {
	got = strlen(ptr);
	if (got >= 1 && ptr[got - 1] == '\n') {
	    tcpd_context.line++;
	    if (got >= 2 && ptr[got - 2] == '\\') {
		got -= 2;
	    } else {
		return (start);
	    }
	}
	ptr += got;
	len -= got;
	ptr[0] = 0;
    }
    return (ptr > start ? start : 0);
}

/* split_at - break string at delimiter or return NULL */

char   *split_at(string, delimiter)
char   *string;
int     delimiter;
{
    char   *cp;

    if ((cp = strchr(string, delimiter)) != 0)
	*cp++ = 0;
    return (cp);
}

/* dot_quad_addr - convert dotted quad to internal form */

in_addr_t dot_quad_addr(str)
char   *str;
{
    int     in_run = 0;
    int     runs = 0;
    char   *cp = str;

    /* Count the number of runs of non-dot characters. */

    while (*cp) {
	if (*cp == '.') {
	    in_run = 0;
	} else if (in_run == 0) {
	    in_run = 1;
	    runs++;
	}
	cp++;
    }
    return (runs == 4 ? inet_addr(str) : INADDR_NONE);
}
