# $OpenBSD: Makefile,v 1.1 2024/02/20 09:40:58 claudio Exp $

.PATH:	${.CURDIR}/../../../usr.bin/rsync

PROGS += rmatch_test

.for p in ${PROGS}
REGRESS_TARGETS +=	run-regress-$p
.endfor

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../../../usr.bin/rsync

SRCS_rmatch_test =	rmatch_test.c rmatch.c

.include <bsd.regress.mk>
