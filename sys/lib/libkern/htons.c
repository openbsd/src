/*	$OpenBSD: htons.c,v 1.4 2004/08/07 00:38:32 deraadt Exp $	*/
/*	$NetBSD: htons.c,v 1.6.6.1 1996/05/29 23:48:02 cgd Exp $	*/

/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$NetBSD: htons.c,v 1.6.6.1 1996/05/29 23:48:02 cgd Exp $";
#endif

#include <sys/types.h>
#include <machine/endian.h>

#undef htons

u_int16_t
htons(u_int16_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *) &x;
	return (u_int16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}
