#	$OpenBSD: Makefile.inc,v 1.2 1997/07/11 04:29:20 millert Exp $

# hash functions
.PATH: ${.CURDIR}/hash

SRCS+=	sha1.c
MAN+=	sha1.3
MLINKS+=sha1.3 SHA1Init.3
MLINKS+=sha1.3 SHA1Update.3
MLINKS+=sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1Transform.3
