# Test DH:

PROG=		dhtest
SRCS=		math_2n.c math_ec2n.c math_group.c dh.c dhtest.c util.c \
		log.c sysdep.c gmp_util.c
.PATH:		${.CURDIR}/../../
NOMAN=
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
TOPOBJ!=	cd ${.CURDIR}/../..; printf "all:\n\t@pwd\n" |${MAKE} -f-
CFLAGS+=	-I${.CURDIR}/../../ -I${TOPOBJ} -Wall
DEBUG=		-g

.include <bsd.prog.mk>
