#	$OpenBSD: Makefile,v 1.1.1.1 2007/07/06 00:42:12 jakemsr Exp $

PROG=	audiotest_gsinfo
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
MAN1=	audiotest_gsinfo.1

.ifndef DO_AUTEST
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
