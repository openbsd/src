#	$OpenBSD: Makefile.inc,v 1.1 1996/06/03 22:20:39 niklas Exp $

.PATH: ${.CURDIR}/md

SRCS+=	md2c.c md4c.c md5c.c md2hl.c md4hl.c md5hl.c

MAN+=	md2.3 md4.3 md5.3
MLINKS+=md2.3 MD2Init.3 md2.3 MD2Update.3 md2.3 MD2Final.3
MLINKS+=md2.3 MD2End.3  md2.3 MD2File.3   md2.3 MD2Data.3
MLINKS+=md4.3 MD4Init.3 md4.3 MD4Update.3 md4.3 MD4Final.3
MLINKS+=md4.3 MD4End.3  md4.3 MD4File.3   md4.3 MD4Data.3
MLINKS+=md5.3 MD5Init.3 md5.3 MD5Update.3 md5.3 MD5Final.3
MLINKS+=md5.3 MD5End.3  md5.3 MD5File.3   md5.3 MD5Data.3
CLEANFILES+=	md[245]hl.c md[245].3

all:	md2.3 md4.3 md5.3

md2hl.c: mdXhl.c
	sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' $> > $@

md4hl.c: mdXhl.c
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' $> > $@

md5hl.c: mdXhl.c
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' $> > $@

md2.3:	mdX.3
	sed -e 's/mdX/md2/g' -e 's/MDX/MD2/g' $> > $@
	cat ${>:H}/md2.copyright >> $@

md4.3:	mdX.3
	sed -e 's/mdX/md4/g' -e 's/MDX/MD4/g' $> > $@
	cat ${>:H}/md4.copyright >> $@

md5.3:	mdX.3
	sed -e 's/mdX/md5/g' -e 's/MDX/MD5/g' $> > $@
	cat ${>:H}/md5.copyright >> $@

beforedepend:	md2hl.c md4hl.c md5hl.c
