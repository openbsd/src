#	$Id: Makefile,v 1.1.1.1 1997/11/23 20:27:39 brian Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall -Wmissing-prototypes
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}
MAN=	pppctl.8

.include <bsd.prog.mk>
