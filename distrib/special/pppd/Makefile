#	$OpenBSD: Makefile,v 1.4 2009/10/28 07:36:49 deraadt Exp $
#	$NetBSD: Makefile,v 1.13 1996/12/18 16:54:57 christos Exp $

PROG=	pppd
SRCS=	auth.c cbcp.c ccp.c chap.c chap_ms.c demand.c fsm.c ipcp.c \
	lcp.c magic.c main.c options.c sys-bsd.c upap.c

.PATH:	${.CURDIR}/../../../usr.sbin/pppd ${.CURDIR}/../../sys/net
BINMODE=4555
BINOWN=	root

LDADD=	-lutil
DPADD=	${LIBUTIL}
CFLAGS+= -I. -DHAVE_PATHS_H

.include <bsd.prog.mk>
