# RSA Key Generation

PROG=		rsakeygen
SRCS=		log.c asn.c gmp_util.c pkcs.c rsakeygen.c sysdep.c
.PATH:		${.CURDIR}/../../
LDADD+=		-lgmp
DPADD+=		${LIBDES}
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>
