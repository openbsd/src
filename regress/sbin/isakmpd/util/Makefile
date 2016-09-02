#	$OpenBSD: Makefile,v 1.2 2016/09/02 16:54:28 mikeb Exp $

# Test some utility functions

PROG=		utiltest
SRCS=		log.c util.c utiltest.c conf.c monitor.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${.CURDIR}/../common
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.regress.mk>
