#	$OpenBSD: Makefile,v 1.5 2000/02/25 17:23:35 niklas Exp $
#	$EOM: Makefile,v 1.5 2000/02/20 19:58:43 niklas Exp $

# Test Crypto:

PROG=		cryptotest
SRCS=		crypto.c cryptotest.c log.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall \
		-DUSE_TRIPLEDES -DUSE_CAST -DUSE_BLOWFISH
LDADD+=		-ldes
DPADD+=		${LIBDES}
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
