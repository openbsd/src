#	$OpenBSD: Makefile.inc,v 1.4 1997/07/17 05:37:01 millert Exp $

# hash functions
.PATH: ${.CURDIR}/hash

SRCS+=	sha1.c sha1hl.c rmd160.c rmd160hl.c
MAN+=	sha1.3
MLINKS+=sha1.3 SHA1Init.3
MLINKS+=sha1.3 SHA1Update.3
MLINKS+=sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1Transform.3
