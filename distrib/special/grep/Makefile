#	$OpenBSD: Makefile,v 1.2 2003/06/22 22:24:41 deraadt Exp $

PROG=	grep
SRCS=	binary.c file.c grep.c mmfile.c queue.c util.c
MAN=
COPTS+=-Os -DNOZ
.PATH:  ${.CURDIR}/../../../usr.bin/grep

#LDADD=-lz
#DPADD=${LIBZ}

.include <bsd.prog.mk>
