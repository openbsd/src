#	$OpenBSD: Makefile.inc,v 1.3 1997/07/12 20:06:02 millert Exp $

# hash functions
.PATH: ${.CURDIR}/hash

SRCS+=	sha1.c sha1hl.c
MAN+=	sha1.3
MLINKS+=sha1.3 SHA1Init.3
MLINKS+=sha1.3 SHA1Update.3
MLINKS+=sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1Transform.3
