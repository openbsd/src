#	$OpenBSD: Makefile,v 1.4 2016/09/03 10:56:58 bluhm Exp $
#	$EOM: Makefile,v 1.7 2000/03/28 21:22:06 ho Exp $

# Test Crypto:

PROG=		cryptotest
SRCS=		crypto.c cryptotest.c conf.c log.c monitor.c util.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${.CURDIR}/../common
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall \
		-DUSE_TRIPLEDES -DUSE_CAST -DUSE_BLOWFISH -DUSE_DES \
		-DUSE_AES
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
NOMAN=
DEBUG=		-g

.PHONY:	isakmpd-depend

all regress: isakmpd-depend

isakmpd-depend:
	cd ${TOPSRC} && ${MAKE} depend

.include <bsd.regress.mk>
