# Test ASN

PROG=		asntest
SRCS=		asntest.c conf.c asn.c asn_useful.c gmp_util.c log.c pkcs.c \
		sysdep.c hash.c x509.c
TOPOBJ!=	cd ${.CURDIR}/../..; printf "all:\n\t@pwd\n" |${MAKE} -f-
.PATH:		${.CURDIR}/../.. ${TOPOBJ}
LDADD+=		-lgmp
DPADD+=		${LIBDES}
NOMAN=
CFLAGS+=	-I${.CURDIR}/../.. -I${TOPOBJ} -Wall
DEBUG=		-g

.include <bsd.prog.mk>
