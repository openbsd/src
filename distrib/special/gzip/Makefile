#	$OpenBSD: Makefile,v 1.3 2003/12/16 22:40:58 henning Exp $

PROG=	gzip
SRCS=	main.c gzopen.c crc32.c
SRCS+=	adler32.c inflate.c inffast.c inftrees.c zutil.c
MAN=
COPTS+=-Os -DSMALL -DDYNAMIC_CRC_TABLE
.PATH:  ${.CURDIR}/../../../usr.bin/compress
.PATH:  ${.CURDIR}/../../../lib/libz

.include <bsd.prog.mk>
