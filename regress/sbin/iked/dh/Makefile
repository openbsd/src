#	$OpenBSD: Makefile,v 1.3 2020/11/03 20:45:58 tobhe Exp $

# Test DH:

PROG=		dhtest
SRCS=		dh.c dhtest.c smult_curve25519_ref.c imsg_util.c
TOPSRC=		${.CURDIR}/../../../../sbin/iked
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${TOPSRC} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ} -Wall

NOMAN=
LDADD+=		-lcrypto -lutil
DPADD+=		${LIBCRYPTO}
DEBUG=		-g

.include <bsd.regress.mk>
