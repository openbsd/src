#	$OpenBSD: Makefile,v 1.5 2000/04/07 22:06:11 niklas Exp $
#	$EOM: Makefile,v 1.8 2000/03/28 21:22:06 ho Exp $

# Test EC2N:

PROG=		ec2ntest
SRCS=		math_2n.c math_ec2n.c ec2ntest.c util.c sysdep.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.prog.mk>
