/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: htons.c,v 1.4 1996/08/19 08:29:05 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <machine/endian.h>

#undef htons

unsigned short
#if __STDC__
htons(unsigned short x)
#else
htons(x)
	unsigned short x;
#endif
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *) &x;
	return (u_int16_t)(s[0] << 8 | s[1]);
#else
	return x;
#endif
}
