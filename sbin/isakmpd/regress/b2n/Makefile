
# Test HMAC:

PROG=		b2ntest
SRCS=		math_2n.c b2ntest.c util.c sysdep.c
.PATH:		${.CURDIR}/../../
NOMAN=
TOPOBJ!=	cd ${.CURDIR}/../..; printf "all:\n\t@pwd\n" |${MAKE} -f-
CFLAGS+=	-I${TOPOBJ} -I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>