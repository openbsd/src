#	$OpenBSD: Makefile,v 1.12 2004/06/25 00:44:02 msf Exp $

PROG=	keynote
SRCS=	keynote-sign.c keynote-keygen.c keynote-sigver.c keynote-verify.c \
	keynote-main.c z.tab.c lex.kv.c
CFLAGS+= -Wall -DCRYPTO -DHAVE_CONFIG_H -I. -I${.CURDIR} -I${.CURDIR}/../../lib/libkeynote
LEXFLAGS= -Pkv -s -i
YACCFLAGS= -d -p kv -b z
DPADD=	${LIBKEYNOTE} ${LIBM} ${LIBCRYPTO}
LDADD=	-lkeynote -lm -lcrypto
CLEANFILES+= lex.kv.c z.tab.c z.tab.h
MAN=	keynote.1

.PATH:	${.CURDIR}/../../lib/libkeynote

z.tab.c: keynote-ver.y
	$(YACC.y) $(YACCFLAGS) ${.ALLSRC}

lex.kv.c: keynote-ver.l
	$(LEX.l) $(LEXFLAGS) ${.ALLSRC}

.include <bsd.prog.mk>
