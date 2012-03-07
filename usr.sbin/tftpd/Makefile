#	$OpenBSD: Makefile,v 1.3 2012/03/07 22:24:01 dlg Exp $

PROG=	tftpd
SRCS=	tftpd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror -pedantic

MAN=	tftpd.8

.include <bsd.prog.mk>
