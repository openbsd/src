#	$OpenBSD: Makefile,v 1.5 2000/02/25 17:23:36 niklas Exp $
#	$EOM: Makefile,v 1.4 2000/02/20 19:58:44 niklas Exp $

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
