#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: infinity.c,v 1.3 2000/10/10 12:39:29 hugh Exp $";
#endif /* LIBC_SCCS and not lint */

/*
 * XXX - This is not correct, but what can we do about it?
 */

/* infinity.c */

#include <math.h>

/* The highest D float on a vax. */
char __infinity[] = { (char)0xff, (char)0x7f, (char)0xff, (char)0xff, 
	(char)0xff, (char)0xff, (char)0xff, (char)0xff };
