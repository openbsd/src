#	$OpenBSD: Makefile,v 1.1 2005/04/08 17:12:49 cloder Exp $
#	$EOM: Makefile,v 1.12 2000/04/07 20:19:43 niklas Exp $

# Test Group:

PROG=		grouptest
SRCS=		math_2n.c math_ec2n.c math_group.c grouptest.c log.c util.c \
		sysdep.c gmp_util.c conf.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
FEATURES!=	awk '/^FEATURES=/ { print $$0 }' ${.CURDIR}/../../Makefile | sed 's/FEATURES=.//'
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall \
		-DUSE_EC
NOMAN=
LDADD+=		-lcrypto
DPADD+=		${LIBCRYPTO}
DEBUG=		-g

.if ${FEATURES:Mgmp} == "gmp"
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_GMP
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
.else
CFLAGS+=	-DMP_FLAVOUR=MP_FLAVOUR_OPENSSL
.endif

.include <bsd.regress.mk>
