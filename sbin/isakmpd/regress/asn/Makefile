# Test ASN

PROG=		asntest
SRCS=		asntest.c conf.c asn.c asn_useful.c gmp_util.c log.c pkcs.c \
		sysdep.c hash.c x509.c
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
