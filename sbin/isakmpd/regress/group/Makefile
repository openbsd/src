#	$OpenBSD: Makefile,v 1.5 2000/02/25 17:23:36 niklas Exp $
#	$EOM: Makefile,v 1.9 2000/02/20 19:58:43 niklas Exp $

# Test Group:

PROG=		grouptest
SRCS=		math_2n.c math_ec2n.c math_group.c grouptest.c log.c util.c \
		sysdep.c gmp_util.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall \
		-DUSE_EC
NOMAN=
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
DEBUG=		-g

.include <bsd.prog.mk>
