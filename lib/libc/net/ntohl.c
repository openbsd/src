/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: ntohl.c,v 1.3 1996/08/19 08:29:33 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

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
	return (u_int32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return y;
#endif
}
