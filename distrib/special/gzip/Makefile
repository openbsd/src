#	$OpenBSD: Makefile,v 1.6 2003/12/17 00:41:04 henning Exp $

PROG=	gzip
SRCS=	main.c gzopen.c 
SRCS+=	adler32.c crc32.c inflate.c inffast.c inftrees.c zutil.c
MAN=
COPTS+=-Os -DSMALL -DDYNAMIC_CRC_TABLE -DNOBYFOUR -DNO_GZIP
.PATH:  ${.CURDIR}/../../../usr.bin/compress
.PATH:  ${.CURDIR}/../../../lib/libz

.include <bsd.prog.mk>
