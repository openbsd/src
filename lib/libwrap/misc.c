/*	$OpenBSD: misc.c,v 1.7 2009/10/27 23:59:30 deraadt Exp $	*/

 /*
  * Misc routines that are used by tcpd and by tcpdchk.
  * 
  * Author: Wietse Venema, Eindhoven University of Technology, The Netherlands.
  */

#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "tcpd.h"

/* xgets - fgets() with backslash-newline stripping */

char   *xgets(ptr, len, fp)
char   *ptr;
int     len;
FILE   *fp;
{
    int     got;
    char   *start = ptr;

    while (len > 1 && fgets(ptr, len, fp)) {
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
    char *cp;
    int bracket;

    bracket = 0;
    for (cp = string; cp && *cp; cp++) {
	switch (*cp) {
	case '[':
	    bracket++;
	    break;
	case ']':
	    bracket--;
	    break;
	default:
	    if (bracket == 0 && *cp == delimiter) {
		*cp++ = 0;
		return cp;
	    }
	    break;
	}
    }
    return NULL;
}

/* dot_quad_addr_new - convert dotted quad to internal form */
int
dot_quad_addr_new(str, ap)
char   *str;
in_addr_t *ap;
{
    struct in_addr a;
    int in_run = 0;
    int runs = 0;
    char *cp = str;

    while (*cp) {
	if (*cp == '.') {
	    in_run = 0;
	} else if (in_run == 0) {
	    in_run = 1;
	    runs++;
	}
	cp++;
    }
    if (runs != 4)
	return 0;

    if (!inet_aton(str, &a))
	return 0;
    if (ap)
	*ap = a.s_addr;
    return 1;
}

/* dot_quad_addr - convert dotted quad to internal form */
in_addr_t
dot_quad_addr(str)
char   *str;
{
    in_addr_t addr;

    (void) dot_quad_addr_new(str, &addr);
    return addr;
}
