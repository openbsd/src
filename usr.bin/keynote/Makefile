# $OpenBSD: Makefile,v 1.1.1.1 1999/05/24 01:30:20 angelos Exp $

PROG=	keynote-sign
LINKS=	${BINDIR}/keynote-sign ${BINDIR}/keynote-sigver
LINKS+=	${BINDIR}/keynote-sign ${BINDIR}/keynote-verify
LINKS+=	${BINDIR}/keynote-sign ${BINDIR}/keynote-keygen

BINOWN= root
BINGRP= bin
BINMODE= 555
BINDIR= /usr/bin
LDADD=	-lkeynote -lm -lcrypto
MAN=	keynote-sign.1 keynote-verify.1 keynote-sigver.1 keynote-keygen.1
CLEANFILES+= lex.kv.c z.tab.c z.tab.h

CFLAGS+= -DCRYPTO -I. -I${.CURDIR}
LEXFLAGS2 = -Pkv -s -i
YACCFLAGS2 = -d -p kv -b z

KEYNOTEDIST= lib/libkeynote
KEYNOTE_SRC= ${.CURDIR}/../../${KEYNOTEDIST}

SRCS=	keynote-sign.c keynote-keygen.c keynote-sigver.c keynote-verify.c \
	keynote-main.c z.tab.c lex.kv.c

z.tab.c: keynote-ver.y
	$(YACC.y) $(YACCFLAGS2) ${KEYNOTE_SRC}/keynote-ver.y

lex.kv.c: keynote-ver.l
	$(LEX.l) $(LEXFLAGS2) ${KEYNOTE_SRC}/keynote-ver.l

.PATH:	${KEYNOTE_SRC}

.include<bsd.prog.mk>
