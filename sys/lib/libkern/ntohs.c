/*	$NetBSD: ntohs.c,v 1.5 1995/10/07 09:26:39 mycroft Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: ntohs.c,v 1.5 1995/10/07 09:26:39 mycroft Exp $";
#endif

#include <sys/types.h>
#include <machine/endian.h>

#undef ntohs

unsigned short
ntohs(x)
	unsigned short x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *) &x;
	return s[0] << 8 | s[1];
#else
	return x;
#endif
}
