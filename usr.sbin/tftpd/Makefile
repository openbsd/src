#	$OpenBSD: Makefile,v 1.2 2012/03/02 04:51:21 dlg Exp $

PROG=	tftpd
SRCS=	tftpd.c
LDADD=	-levent
DPADD=	${LIBEVENT}
CFLAGS+= -Wall -Werror

MAN=	tftpd.8

.include <bsd.prog.mk>
