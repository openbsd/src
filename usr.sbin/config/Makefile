#	$OpenBSD: Makefile,v 1.2 1996/03/25 15:55:00 niklas Exp $
#	$NetBSD: Makefile,v 1.11 1996/03/03 17:28:02 thorpej Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/19/94

PROG=	config
BINDIR=	/usr/sbin
SRCS=	files.c gram.y hash.c main.c mkheaders.c mkioconf.c mkmakefile.c \
	mkswap.c pack.c scan.l sem.c util.c
CFLAGS+=-I${.CURDIR} -I.
LDADD+=	-ll
DPADD+=	${LIBL}
CLEANFILES=gram.c scan.c y.tab.h
MAN=	config.8

.include <bsd.prog.mk>

.depend: gram.c scan.c
