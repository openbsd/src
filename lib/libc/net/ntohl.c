/*	$OpenBSD: ntohl.c,v 1.6 2005/08/06 20:30:03 espie Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <sys/types.h>
#include <machine/endian.h>

#undef ntohl

u_int32_t
ntohl(u_int32_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *)&x;
	return (u_int32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}
