#	$OpenBSD: Makefile,v 1.5 2003/12/16 23:54:30 henning Exp $

PROG=	gzip
SRCS=	main.c gzopen.c crc32.c
SRCS+=	adler32.c inflate.c inffast.c inftrees.c zutil.c
MAN=
COPTS+=-Os -DSMALL -DDYNAMIC_CRC_TABLE -DNOBYFOUR -DNO_GZIP
.PATH:  ${.CURDIR}/../../../usr.bin/compress
.PATH:  ${.CURDIR}/../../../lib/libz

.include <bsd.prog.mk>
