#	$OpenBSD: Makefile,v 1.3 1998/12/21 01:02:30 niklas Exp $
#	$EOM: Makefile,v 1.4 1998/12/21 00:41:52 niklas Exp $

# Test EC2N:

PROG=		ec2ntest
SRCS=		math_2n.c math_ec2n.c ec2ntest.c util.c sysdep.c
.PATH:		${.CURDIR}/../../
NOMAN=
TOPOBJ!=	cd ${.CURDIR}/../..; printf "all:\n\t@pwd\n" |${MAKE} -f-
CFLAGS+=	-I${TOPOBJ} -I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>
