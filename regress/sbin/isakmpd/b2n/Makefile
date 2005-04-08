#	$OpenBSD: Makefile,v 1.1 2005/04/08 17:12:48 cloder Exp $
#	$EOM: Makefile,v 1.12 2000/10/13 13:04:17 ho Exp $

# Test some math

PROG=		b2ntest
SRCS=		b2ntest.c conf.c log.c math_2n.c sysdep.c util.c
NOMAN=
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
DEBUG=		-g

.include <bsd.regress.mk>
