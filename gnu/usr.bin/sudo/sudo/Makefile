#       $OpenBSD: Makefile,v 1.4 1997/04/19 21:26:13 millert Exp $

PROG=	sudo
MAN=	sudo.8 sudoers.5
CFLAGS+=-I${.CURDIR} -I.
SRCS=	check.c find_path.c getspwuid.c goodpath.c interfaces.c logging.c parse.c sudo.c sudo_setenv.c tgetpass.c y.tab.c lex.yy.c
CLEANFILES+=y.tab.c y.tab.h lex.yy.c

LDADD=  -lcompat
DPADD=  ${LIBCOMPAT}

.include <bsd.own.mk>   # For SKEY, KERBEROS and KERBEROS5

.if (${SKEY} == "yes")
CFLAGS+=-DHAVE_SKEY
LDADD+= -lskey
DPADD+= ${LIBSKEY}
.endif

.if (${KERBEROS5} == "yes")
CFLAGS+= -DHAVE_KERB5 -I${DESTDIR}/usr/include/kerberosIV
LDADD+= -lkrb5 -lcrypto
DPADD+= ${LIBKRB5} ${LIBCRYPTO}
.elif (${KERBEROS} == "yes")
CFLAGS+= -DHAVE_KERB4 -I${DESTDIR}/usr/include/kerberosIV
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
