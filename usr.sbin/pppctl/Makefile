#	$Id: Makefile,v 1.2 1998/07/24 00:11:11 millert Exp $

PROG=	pppctl
SRCS=	pppctl.c
CFLAGS+=-Wall -Wmissing-prototypes
LDADD+= -ledit -lcurses
DPADD+= ${LIBEDIT} ${LIBCURSES}
MAN=	pppctl.8

.include <bsd.prog.mk>
