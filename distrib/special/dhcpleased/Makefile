#	$OpenBSD: Makefile,v 1.2 2021/06/18 11:46:06 florian Exp $

PROG=	dhcpleased
COPTS+=	-DSMALL
.PATH:  ${.CURDIR}/../../../sbin/dhcpleased
SRCS=	bpf.c checksum.c dhcpleased.c engine.c frontend.c

MAN=

LDADD+=	-levent -lutil
DPADD+= ${LIBEVENT} ${LIBUTIL}

.include <bsd.prog.mk>
