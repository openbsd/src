#	$OpenBSD: Makefile,v 1.3 1998/12/21 01:02:28 niklas Exp $
#	$EOM: Makefile,v 1.5 1998/12/21 00:32:58 niklas Exp $

# Test HMAC:

PROG=		b2ntest
SRCS=		math_2n.c b2ntest.c util.c sysdep.c
.PATH:		${.CURDIR}/../../
NOMAN=
TOPOBJ!=	cd ${.CURDIR}/../..; printf "all:\n\t@pwd\n" |${MAKE} -f-
CFLAGS+=	-I${TOPOBJ} -I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>