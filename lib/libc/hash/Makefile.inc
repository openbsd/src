#	$OpenBSD: Makefile.inc,v 1.15 2004/04/26 19:38:12 millert Exp $

# hash functions
.PATH: ${LIBCSRCDIR}/hash

HELPER=	rmd160hl.c sha1hl.c sha256hl.c sha384hl.c sha512hl.c
SRCS+=	rmd160.c sha1.c sha2.c ${HELPER}
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
CLEANFILES+= ${HELPER}

rmd160hl.c:	helper.c
	sed -e 's/hashinc/rmd160.h/g' -e 's/HASH/RMD160/g' $> > $@

sha1hl.c:	helper.c
	sed -e 's/hashinc/sha1.h/g' -e 's/HASH/SHA1/g' $> > $@

sha256hl.c:	helper.c
	sed -e 's/hashinc/sha2.h/g' -e 's/HASH_\{0,1\}/SHA256_/g' $> > $@

sha384hl.c:	helper.c
	sed -e 's/hashinc/sha2.h/g' -e 's/HASH_\{0,1\}/SHA384_/g' $> > $@

sha512hl.c:	helper.c
	sed -e 's/hashinc/sha2.h/g' -e 's/HASH_\{0,1\}/SHA512_/g' $> > $@

beforedepend: rmd160hl.c sha1hl.c sha256hl.c sha384hl.c sha512hl.c
