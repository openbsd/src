#	$OpenBSD: Makefile.inc,v 1.14 2003/05/08 23:34:55 millert Exp $

# hash functions
.PATH: ${LIBCSRCDIR}/hash

SRCS+=	rmd160.c rmd160hl.c sha1.c sha1hl.c sha2.c sha2hl.c
MAN+=	rmd160.3 sha1.3 sha2.3
MLINKS+=rmd160.3 RMD160Init.3 rmd160.3 RMD160Update.3 rmd160.3 RMD160Final.3
MLINKS+=rmd160.3 RMD160End.3  rmd160.3 RMD160File.3   rmd160.3 RMD160Data.3
MLINKS+=rmd160.3 RMD160Transform.3
MLINKS+=sha1.3 SHA1Init.3 sha1.3 SHA1Update.3 sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1End.3  sha1.3 SHA1File.3   sha1.3 SHA1Data.3
MLINKS+=sha1.3 SHA1Transform.3
MLINKS+=sha2.3 SHA256_Init.3 sha2.3 SHA256_Update.3 sha2.3 SHA256_Final.3
MLINKS+=sha2.3 SHA256_End.3  sha2.3 SHA256_File.3   sha2.3 SHA256_Data.3
MLINKS+=sha2.3 SHA384_Init.3 sha2.3 SHA384_Update.3 sha2.3 SHA384_Final.3
MLINKS+=sha2.3 SHA384_End.3  sha2.3 SHA384_File.3   sha2.3 SHA384_Data.3
MLINKS+=sha2.3 SHA512_Init.3 sha2.3 SHA512_Update.3 sha2.3 SHA512_Final.3
MLINKS+=sha2.3 SHA512_End.3  sha2.3 SHA512_File.3   sha2.3 SHA512_Data.3
