#	$OpenBSD: Makefile,v 1.2 2016/09/02 16:54:28 mikeb Exp $
#	$EOM: Makefile,v 1.3 1999/02/25 15:14:24 niklas Exp $

# Test HMAC:

PROG=		hmactest
SRCS=		hash.c hmactest.c conf.c log.c monitor.c util.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${.CURDIR}/../common
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.regress.mk>
