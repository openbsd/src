#	$OpenBSD: Makefile,v 1.5 2012/09/12 09:19:54 haesbaert Exp $

PROG=	tftpd
SRCS=	tftpd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror
CDIAGFLAGS=

MAN=	tftpd.8

.include <bsd.prog.mk>
