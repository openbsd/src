#	$OpenBSD: Makefile,v 1.11 2004/01/05 02:55:28 espie Exp $

PROG=	keynote
SRCS=	keynote-sign.c keynote-keygen.c keynote-sigver.c keynote-verify.c \
	keynote-main.c z.tab.c lex.kv.c
CFLAGS+= -Wall -DCRYPTO -DHAVE_CONFIG_H -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libkeynote
LEXFLAGS2= -Pkv -s -i
YACCFLAGS2= -d -p kv -b z
DPADD=	${LIBKEYNOTE} ${LIBM} ${LIBCRYPTO}
LDADD=	-lkeynote -lm -lcrypto
CLEANFILES+= lex.kv.c z.tab.c z.tab.h
MAN=	keynote.1

.PATH:	${.CURDIR}/../../lib/libkeynote

z.tab.c: keynote-ver.y
	$(YACC.y) $(YACCFLAGS2) $>

lex.kv.c: keynote-ver.l
	$(LEX.l) $(LEXFLAGS2) $>

.include<bsd.prog.mk>
