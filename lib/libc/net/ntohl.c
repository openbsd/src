/*	$NetBSD: ntohl.c,v 1.5 1995/04/28 23:25:21 jtc Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: ntohl.c,v 1.5 1995/04/28 23:25:21 jtc Exp $";
#endif

#include <sys/types.h>
#include <machine/endian.h>

#undef ntohl

unsigned long
ntohl(x)
	unsigned long x;
{
	u_int32_t y = x;

#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *)&y;
	return s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3];
#else
	return y;
#endif
}
