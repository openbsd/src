#	$OpenBSD: Makefile,v 1.3 1997/01/18 02:24:11 briggs Exp $
#	$NetBSD: Makefile,v 1.12 1996/11/11 23:36:40 gwr Exp $
#	from: @(#)Makefile	8.2 (Berkeley) 4/19/94

PROG=	config
BINDIR=	/usr/sbin
SRCS=	files.c gram.y hash.c main.c mkheaders.c mkioconf.c mkmakefile.c \
	mkswap.c pack.c scan.l sem.c util.c
CFLAGS+=-I${.CURDIR} -I.

# This program actually requires "flex" (not just any old lex).
# Also note that it does not use -ll
LEX=flex

CLEANFILES=gram.c scan.c y.tab.h
MAN=	config.8

.include <bsd.prog.mk>

.depend: gram.c scan.c
