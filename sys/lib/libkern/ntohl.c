/*	$OpenBSD: ntohl.c,v 1.4 2004/08/07 00:38:32 deraadt Exp $	*/
/*	$NetBSD: ntohl.c,v 1.6.6.1 1996/05/29 23:48:07 cgd Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: ntohl.c,v 1.6.6.1 1996/05/29 23:48:07 cgd Exp $";
#endif

#include <sys/types.h>
#include <machine/endian.h>

#undef ntohl

u_int32_t
ntohl(u_int32_t x)
{
	u_int32_t y = x;

#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *)&y;
	return (u_int32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return y;
#endif
}
