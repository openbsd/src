#	$OpenBSD: Makefile.inc,v 1.6 1998/11/20 11:18:41 d Exp $

# hash functions
.PATH: ${LIBCSRCDIR}/hash

SRCS+=	sha1.c sha1hl.c rmd160.c rmd160hl.c
MAN+=	sha1.3 rmd160.3
MLINKS+=sha1.3 SHA1Init.3
MLINKS+=sha1.3 SHA1Update.3
MLINKS+=sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1Transform.3
