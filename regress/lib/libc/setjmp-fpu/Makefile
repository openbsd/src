#	$OpenBSD: Makefile,v 1.5 2020/12/17 00:51:11 bluhm Exp $

PROGS=	fpu _setjmp setjmp sigsetjmp

LDADD=	-lm

REGRESS_TARGETS =	run-fpu
run-fpu: fpu
	# check fpu functions without setjmp/longjmp before testing the latter
	./fpu

.for p in ${PROGS:M*jmp}
REGRESS_TARGETS +=	run-$p
run-$p: $p
	./$p
.endfor

.include <bsd.regress.mk>
