#	$OpenBSD: Makefile,v 1.2 2003/09/05 04:46:35 tedu Exp $

PROG=	gzip
SRCS=	main.c gzopen.c
MAN=
COPTS+=-Os -DSMALL
.PATH:  ${.CURDIR}/../../../usr.bin/compress

LDADD=-lz
DPADD=${LIBZ}

.include <bsd.prog.mk>
