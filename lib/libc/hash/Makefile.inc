#	$OpenBSD: Makefile.inc,v 1.18 2008/09/06 12:00:19 djm Exp $

# hash functions
.PATH: ${LIBCSRCDIR}/hash

HELPER=	md4hl.c md5hl.c rmd160hl.c sha1hl.c sha256hl.c sha384hl.c sha512hl.c
SRCS+=	md4.c md5.c rmd160.c sha1.c sha2.c ${HELPER}
MAN+=	md4.3 md5.3 rmd160.3 sha1.3 sha2.3

MLINKS+=md4.3 MD4Transform.3 md4.3 MD4Init.3 md4.3 MD4Final.3
MLINKS+=md4.3 MD4Update.3    md4.3 MD4End.3  md4.3 MD4File.3
MLINKS+=md4.3 MD4FileChunk.3 md4.3 MD4Pad.3  md4.3 MD4Data.3
MLINKS+=md5.3 MD5Transform.3 md5.3 MD5Init.3 md5.3 MD5Final.3
MLINKS+=md5.3 MD5Update.3    md5.3 MD5End.3  md5.3 MD5File.3
MLINKS+=md5.3 MD5FileChunk.3 md5.3 MD5Pad.3  md5.3 MD5Data.3
MLINKS+=rmd160.3 RMD160Transform.3 rmd160.3 RMD160Init.3 rmd160.3 RMD160Final.3
MLINKS+=rmd160.3 RMD160Update.3    rmd160.3 RMD160End.3  rmd160.3 RMD160File.3
MLINKS+=rmd160.3 RMD160FileChunk.3 rmd160.3 RMD160Pad.3  rmd160.3 RMD160Data.3
MLINKS+=sha1.3 SHA1Transform.3 sha1.3 SHA1Init.3 sha1.3 SHA1Final.3
MLINKS+=sha1.3 SHA1Update.3    sha1.3 SHA1End.3  sha1.3 SHA1File.3
MLINKS+=sha1.3 SHA1FileChunk.3 sha1.3 SHA1Pad.3  sha1.3 SHA1Data.3
MLINKS+=sha2.3 SHA256Init.3  sha2.3 SHA256Update.3    sha2.3 SHA256Pad.3
MLINKS+=sha2.3 SHA256Final.3 sha2.3 SHA256Transform.3 sha2.3 SHA256End.3
MLINKS+=sha2.3 SHA256File.3  sha2.3 SHA256FileChunk.3 sha2.3 SHA256Data.3
MLINKS+=sha2.3 SHA384Init.3  sha2.3 SHA384Update.3    sha2.3 SHA384Pad.3
MLINKS+=sha2.3 SHA384Final.3 sha2.3 SHA384Transform.3 sha2.3 SHA384End.3
MLINKS+=sha2.3 SHA384File.3  sha2.3 SHA384FileChunk.3 sha2.3 SHA384Data.3
MLINKS+=sha2.3 SHA512Init.3  sha2.3 SHA512Update.3    sha2.3 SHA512Pad.3
MLINKS+=sha2.3 SHA512Final.3 sha2.3 SHA512Transform.3 sha2.3 SHA512End.3
MLINKS+=sha2.3 SHA512File.3  sha2.3 SHA512FileChunk.3 sha2.3 SHA512Data.3
CLEANFILES+= ${HELPER} md[45].3

.ifndef NOMAN
all:	md4.3 md5.3

md4.3:  mdX.3
	sed -e 's/mdX/md4/g' -e 's/mdY/md5/g' -e 's/MDX/MD4/g' $> > $@

md5.3:  mdX.3
	sed -e 's/mdX/md5/g' -e 's/mdY/md4/g' -e 's/MDX/MD5/g' $> > $@
.endif

md4hl.c:	helper.c
	sed -e 's/hashinc/md4.h/g' -e 's/HASH/MD4/g' $> > $@

md5hl.c:	helper.c
	sed -e 's/hashinc/md5.h/g' -e 's/HASH/MD5/g' $> > $@

rmd160hl.c:	helper.c
	sed -e 's/hashinc/rmd160.h/g' -e 's/HASH/RMD160/g' $> > $@

sha1hl.c:	helper.c
	sed -e 's/hashinc/sha1.h/g' -e 's/HASH/SHA1/g' $> > $@

sha256hl.c:	helper.c
	sed -e 's/hashinc/sha2.h/g' \
	    -e 's/HASH/SHA256/g' \
	    -e 's/SHA[0-9][0-9][0-9]_CTX/SHA2_CTX/g' $> > $@

sha384hl.c:	helper.c
	sed -e 's/hashinc/sha2.h/g' \
	    -e 's/HASH/SHA384/g' \
	    -e 's/SHA[0-9][0-9][0-9]_CTX/SHA2_CTX/g' $> > $@

sha512hl.c:	helper.c
	sed -e 's/hashinc/sha2.h/g' \
	    -e 's/HASH/SHA512/g' \
	    -e 's/SHA[0-9][0-9][0-9]_CTX/SHA2_CTX/g' $> > $@

beforedepend: md4hl.c md5hl.c rmd160hl.c sha1hl.c sha256hl.c sha384hl.c sha512hl.c
