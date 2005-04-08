#	$OpenBSD: Makefile,v 1.1 2005/04/08 17:12:49 cloder Exp $
#	$EOM: Makefile,v 1.9 2000/10/13 13:04:17 ho Exp $

# Test EC2N:

PROG=		ec2ntest
SRCS=		math_2n.c math_ec2n.c ec2ntest.c log.c sysdep.c util.c conf.c
TOPSRC=		${.CURDIR}/../../../../sbin/isakmpd
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/openbsd ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/openbsd -I${TOPOBJ} -Wall
NOMAN=
DEBUG=		-g

.include <bsd.regress.mk>
