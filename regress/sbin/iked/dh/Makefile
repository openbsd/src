#	$OpenBSD: Makefile,v 1.1 2014/08/25 19:22:20 reyk Exp $

# Test DH:

PROG=		dhtest
SRCS=		dh.c dhtest.c
TOPSRC=		${.CURDIR}/../../../../sbin/iked
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${TOPSRC} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPOBJ} -Wall

NOMAN=
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
DEBUG=		-g

.include <bsd.regress.mk>
