#	$Id: Makefile,v 1.2 1998/09/17 00:47:14 brian Exp $

PROG=	pppctl
SRCS=	pppctl.c
MAN=	pppctl.8
CFLAGS+=-Wall
LDADD+= -ledit -ltermcap
DPADD+= ${LIBEDIT} ${LIBTERMCAP}

.include <bsd.prog.mk>
