#	$OpenBSD: Makefile,v 1.3 2004/07/09 19:48:02 david Exp $

PROG=autest
SRCS=autest.c adpcm.c law.c
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
MAN1=autest.1
LDADD=-lm

.ifndef DO_AUTEST
REGRESS_SKIP=
.endif

.include <bsd.regress.mk>
