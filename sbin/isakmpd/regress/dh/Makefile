#	$OpenBSD: Makefile,v 1.7 2001/01/27 11:24:41 niklas Exp $
#	$EOM: Makefile,v 1.10 2000/04/07 20:19:43 niklas Exp $

# Test DH:

PROG=		dhtest
SRCS=		math_2n.c math_ec2n.c math_group.c dh.c dhtest.c log.c util.c \
		sysdep.c gmp_util.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
FEATURES!=	awk '/^FEATURES=/ { print $$0 }' ${.CURDIR}/../../Makefile | sed 's/FEATURES=.//'
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall \
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

.include <bsd.prog.mk>
