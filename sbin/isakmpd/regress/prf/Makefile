#	$OpenBSD: Makefile,v 1.3 1998/12/21 01:02:34 niklas Exp $
#	$EOM: Makefile,v 1.2 1998/12/21 00:41:57 niklas Exp $

# Test PRF:

PROG=		prftest
SRCS=		log.c prf.c hash.c prftest.c
.PATH:		${.CURDIR}/../../
NOMAN=
CFLAGS+=	-I${.CURDIR}/../../ -Wall
DEBUG=		-g

.include <bsd.prog.mk>
