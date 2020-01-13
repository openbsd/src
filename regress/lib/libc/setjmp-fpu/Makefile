#	$OpenBSD: Makefile,v 1.3 2020/01/13 14:58:38 bluhm Exp $

PROGS=	_setjmp setjmp sigsetjmp

LDADD=	-lm

.for p in ${PROGS}
REGRESS_TARGETS +=	run-$p
run-$p: $p
	@echo '======== $@ ========'
	./$p
.endfor

.include <bsd.regress.mk>
