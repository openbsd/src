#       $OpenBSD: Makefile,v 1.1 1996/10/14 05:14:43 millert Exp $

PROG=	sudo
MAN=	sudo.8 sudoers.5
CFLAGS+=-I${.CURDIR} -I.
SRCS=	check.c find_path.c getspwuid.c goodpath.c interfaces.c logging.c parse.c sudo.c sudo_setenv.c tgetpass.c y.tab.c lex.yy.c
CLEANFILES+=y.tab.c y.tab.h lex.yy.c

LDADD=  -lcompat
DPADD=  ${LIBCOMPAT}

.include <bsd.own.mk>   # For SKEY, KERBEROS and KERBEROS5

.if defined(SKEY)
CFLAGS+=-DHAVE_SKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if defined(KERBEROS5)
CFLAGS+= -DHAVE_KERB5
LDADD+= -lkrb5 -lcrypto
DPADD+= ${LIBKRB5} ${LIBCRYPTO}
.elif defined(KERBEROS)
CFLAGS+= -DHAVE_KERB4
LDADD+= -lkrb -ldes
DPADD+= ${LIBKRB} ${LIBDES}
.endif

BINOWN=	root
BINMODE=4111
BINDIR?=/usr/bin

.include <bsd.prog.mk>

lex.yy.c: parse.lex
	rm -f lex.yy.c
	$(LEX) ${.CURDIR}/parse.lex

y.tab.c y.tab.h: parse.yacc
	$(YACC) -d ${.CURDIR}/parse.yacc
