#	$OpenBSD: Makefile,v 1.2 2014/08/27 10:28:57 reyk Exp $

# Test DH:

PROG=		dhtest
SRCS=		dh.c dhtest.c smult_curve25519_ref.c
TOPSRC=		${.CURDIR}/../../../../sbin/iked
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${TOPSRC} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ} -Wall

NOMAN=
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
DEBUG=		-g

.include <bsd.regress.mk>
