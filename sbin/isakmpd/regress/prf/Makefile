#	$OpenBSD: Makefile,v 1.6 2000/04/07 22:06:11 niklas Exp $
#	$EOM: Makefile,v 1.6 2000/03/28 21:22:07 ho Exp $

# Test PRF:

PROG=		prftest
SRCS=		prf.c hash.c log.c prftest.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
