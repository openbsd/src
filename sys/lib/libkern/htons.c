/*	$NetBSD: htons.c,v 1.6 1995/10/07 09:26:27 mycroft Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: htons.c,v 1.6 1995/10/07 09:26:27 mycroft Exp $";
#endif

#include <sys/types.h>
#include <machine/endian.h>

#undef htons

unsigned short
htons(x)
	unsigned short x;
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *) &x;
	return s[0] << 8 | s[1];
#else
	return x;
#endif
}
