#	$OpenBSD: Makefile,v 1.1 2005/04/08 17:12:48 cloder Exp $
#	$EOM: Makefile,v 1.7 2000/03/28 21:22:06 ho Exp $

# Test Crypto:

PROG=		cryptotest
SRCS=		crypto.c cryptotest.c conf.c log.c sysdep.c util.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall \
		-DUSE_TRIPLEDES -DUSE_CAST -DUSE_BLOWFISH -DUSE_DES \
		-DUSE_AES
LDADD+=		-lcrypto -ldes
DPADD+=		${LIBCRYPTO} ${LIBDES}
NOMAN=
DEBUG=		-g

.include <bsd.regress.mk>
