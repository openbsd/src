#	$OpenBSD: Makefile,v 1.4 2012/03/08 03:17:10 dlg Exp $

PROG=	tftpd
SRCS=	tftpd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror

MAN=	tftpd.8

.include <bsd.prog.mk>
