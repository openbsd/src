#	$OpenBSD: Makefile.inc,v 1.5 1997/07/17 06:02:42 millert Exp $

# hash functions
.PATH: ${.CURDIR}/hash

SRCS+=	sha1.c sha1hl.c rmd160.c rmd160hl.c
MAN+=	sha1.3 rmd160.3
MLINKS+=sha1.3 SHA1Init.3
MLINKS+=sha1.3 SHA1Update.3
MLINKS+=sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1Transform.3
