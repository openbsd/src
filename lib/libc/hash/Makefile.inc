#	$OpenBSD: Makefile.inc,v 1.13 2003/01/08 19:53:59 millert Exp $

# hash functions
.PATH: ${LIBCSRCDIR}/hash

SRCS+=	sha1.c sha1hl.c rmd160.c rmd160hl.c
MAN+=	sha1.3 rmd160.3
MLINKS+=sha1.3 SHA1Init.3 sha1.3 SHA1Update.3 sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1End.3  sha1.3 SHA1File.3   sha1.3 SHA1Data.3
MLINKS+=sha1.3 SHA1Transform.3
MLINKS+=rmd160.3 RMD160Init.3 rmd160.3 RMD160Update.3 rmd160.3 RMD160Final.3
MLINKS+=rmd160.3 RMD160End.3  rmd160.3 RMD160File.3   rmd160.3 RMD160Data.3
MLINKS+=rmd160.3 RMD160Transform.3
