#	$OpenBSD: Makefile,v 1.4 1999/02/26 03:27:14 niklas Exp $
#	$EOM: Makefile,v 1.6 1999/02/25 15:10:06 niklas Exp $

# Test DH:

PROG=		dhtest
SRCS=		math_2n.c math_ec2n.c math_group.c dh.c dhtest.c util.c \
		log.c sysdep.c gmp_util.c
TOPSRC=		${.CURDIR}/../..
TOPOBJ!=	cd ${TOPSRC}; printf "all:\n\t@pwd\n" |${MAKE} -f-
OS!=		awk '/^OS=/ { print $$2 }' ${.CURDIR}/../../Makefile
.PATH:		${TOPSRC} ${TOPSRC}/sysdep/${OS} ${TOPOBJ}
CFLAGS+=	-I${TOPSRC} -I${TOPSRC}/sysdep/${OS} -I${TOPOBJ} -Wall
NOMAN=
LDADD+=		-lgmp
DPADD+=		${LIBGMP}
DEBUG=		-g

.include <bsd.prog.mk>
