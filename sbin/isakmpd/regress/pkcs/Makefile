#	$OpenBSD: Makefile,v 1.5 1999/03/02 15:27:36 niklas Exp $
#	$EOM: Makefile,v 1.5 1999/02/25 15:12:00 niklas Exp $

# Test PKCS#1

PROG=		pkcstest
SRCS=		log.c asn.c gmp_util.c pkcs.c pkcstest.c sysdep.c \
		asn_useful.c hash.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
LDADD+=		-lgmp
DPADD+=		${LIBDES}
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
