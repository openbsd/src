#	$OpenBSD: Makefile,v 1.1 2003/06/22 22:25:23 deraadt Exp $

PROG=	gzip
SRCS=	main.c zopen.c gzopen.c
MAN=
COPTS+=-Os
.PATH:  ${.CURDIR}/../../../usr.bin/compress

LDADD=-lz
DPADD=${LIBZ}

.include <bsd.prog.mk>
