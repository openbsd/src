#	$OpenBSD: Makefile,v 1.2 2003/02/01 18:18:06 jason Exp $

PROG=autest
SRCS=autest.c adpcm.c law.c
CFLAGS+=-Wall -Wstrict-prototypes -Wmissing-prototypes
MAN1=autest.1
LDADD=-lm

.ifndef DO_AUTEST
REGRESS_SKIP=
REGRESS_SKIP_TARGETS=autest
.endif

.include <bsd.regress.mk>
