#	$OpenBSD: Makefile,v 1.11 2004/02/25 16:01:29 hshoexer Exp $
#	$EOM: Makefile,v 1.7 2000/03/28 21:22:06 ho Exp $

# Test Crypto:

PROG=		cryptotest
SRCS=		crypto.c cryptotest.c conf.c log.c sysdep.c util.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall \
		-DUSE_TRIPLEDES -DUSE_CAST -DUSE_BLOWFISH -DUSE_DES \
		-DUSE_AES
LDADD+=		-lcrypto -ldes
DPADD+=		${LIBCRYPTO} ${LIBDES}
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
