#	$OpenBSD: Makefile,v 1.10 2002/05/11 00:20:20 espie Exp $

PROG=	keynote
SRCS=	keynote-sign.c keynote-keygen.c keynote-sigver.c keynote-verify.c \
	keynote-main.c z.tab.c lex.kv.c
CFLAGS+= -Wall -DCRYPTO -DHAVE_CONFIG_H -I. -I${.CURDIR}
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
