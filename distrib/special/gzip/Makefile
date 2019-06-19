#	$OpenBSD: Makefile,v 1.9 2018/06/25 16:29:00 deraadt Exp $

PROG=	gzip
SRCS=	main.c gzopen.c
SRCS+=	adler32.c crc32.c inflate.c inftrees.c zutil.c
COPTS+=	-DSMALL -DDYNAMIC_CRC_TABLE -DNOBYFOUR -DNO_GZIP -DSLOW
.PATH:  ${.CURDIR}/../../../usr.bin/compress
.PATH:  ${.CURDIR}/../../../lib/libz

.include <bsd.prog.mk>
