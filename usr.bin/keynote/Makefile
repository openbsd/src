# $OpenBSD: Makefile,v 1.6 1999/08/14 20:36:07 angelos Exp $

PROG=	keynote

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote.1
CLEANFILES+= lex.kv.c z.tab.c z.tab.h

CFLAGS+= -Wall -DCRYPTO -I. -I${.CURDIR} -I/usr/include/ssl
LEXFLAGS2 = -Pkv -s -i
YACCFLAGS2 = -d -p kv -b z

KEYNOTEDIST= lib/libkeynote
KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

SRCS=	keynote-sign.c keynote-keygen.c keynote-sigver.c keynote-verify.c \
	keynote-main.c z.tab.c lex.kv.c

z.tab.c: keynote-ver.y keynote.h header.h
	$(YACC.y) $(YACCFLAGS2) ${KEYNOTE_SRC}/keynote-ver.y

lex.kv.c: keynote-ver.l keynote.h header.h
	$(LEX.l) $(LEXFLAGS2) ${KEYNOTE_SRC}/keynote-ver.l

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>
