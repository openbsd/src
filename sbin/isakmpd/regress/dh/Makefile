#	$OpenBSD: Makefile,v 1.3 1998/12/21 01:02:29 niklas Exp $
#	$EOM: Makefile,v 1.4 1998/12/21 00:41:51 niklas Exp $

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
