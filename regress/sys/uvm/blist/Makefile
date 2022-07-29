# $OpenBSD: Makefile,v 1.1 2022/07/29 17:47:11 semarie Exp $

SRCS +=		${.CURDIR}/../../../../sys/kern/subr_blist.c
WARNINGS =	Yes

TESTS +=	\
	test-1	1024 \
	test-2	1024 \
	test-3	64 \
	test-4	64 \
	test-5	1024

.for t s in ${TESTS}
run-$t: blist
	./blist $s <${.CURDIR}/$t.in >$t.run
	diff -u ${.CURDIR}/$t.out $t.run

show-$t: blist
	./blist $s <${.CURDIR}/$t.in 2>&1

regen-$t: blist
	./blist $s <${.CURDIR}/$t.in >${.CURDIR}/$t.out

REGRESS_TARGETS +=	run-$t
REGEN_TARGETS +=	regen-$t
CLEANFILES +=		$t.run
.endfor

blist: ${SRCS}
	${CC} -o $@ ${CFLAGS} ${SRCS}
CLEANFILES +=	blist blist.d

regen: ${REGEN_TARGETS}

.PHONY: regen ${REGEN_TARGETS}

.include <bsd.regress.mk>
