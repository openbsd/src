#	$OpenBSD: Makefile,v 1.1 2021/05/19 17:16:51 deraadt Exp $

PROG=	dhcpleased
.PATH:  ${.CURDIR}/../../../sbin/dhcpleased
SRCS=	bpf.c checksum.c control.c dhcpleased.c engine.c frontend.c log.c

MAN=

LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
