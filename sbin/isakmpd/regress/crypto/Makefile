#	$OpenBSD: Makefile,v 1.4 1999/02/26 03:27:01 niklas Exp $
#	$EOM: Makefile,v 1.4 1999/02/25 15:14:23 niklas Exp $

# Test Crypto:

PROG=		cryptotest
SRCS=		log.c crypto.c cryptotest.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
LDADD+=		-ldes
DPADD+=		${LIBDES}
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
