#	$OpenBSD: Makefile,v 1.4 1999/02/26 03:27:31 niklas Exp $
#	$EOM: Makefile,v 1.6 1999/02/25 15:10:08 niklas Exp $

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
