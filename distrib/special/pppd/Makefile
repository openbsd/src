#	$OpenBSD: Makefile,v 1.2 1999/06/17 20:42:30 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1996/12/18 16:54:57 christos Exp $

COPTS+=-Os

PROG=	pppd
SRCS=	auth.c cbcp.c ccp.c chap.c chap_ms.c demand.c fsm.c ipcp.c \
	ipxcp.c lcp.c magic.c main.c options.c sys-bsd.c upap.c

.PATH:	${.CURDIR}/../../../usr.sbin/pppd ${.CURDIR}/../../sys/net
MAN=
BINMODE=4555
BINOWN=	root

LDADD=	-lutil
DPADD=	${LIBUTIL}
CFLAGS+= -I. -DHAVE_PATHS_H

.include <bsd.prog.mk>
