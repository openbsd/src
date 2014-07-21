/*	$OpenBSD: htonl.c,v 1.7 2014/07/21 01:51:10 guenther Exp $ */
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <sys/types.h>
#include <endian.h>

#undef htonl

u_int32_t
htonl(u_int32_t x)
{
#if BYTE_ORDER == LITTLE_ENDIAN
	u_char *s = (u_char *)&x;
	return (u_int32_t)(s[0] << 24 | s[1] << 16 | s[2] << 8 | s[3]);
#else
	return x;
#endif
}
