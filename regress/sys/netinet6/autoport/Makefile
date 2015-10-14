#	$OpenBSD: Makefile,v 1.1 2015/10/14 12:38:52 vgross Exp $

REGRESS_TARGETS=	run-regress-autoport
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-autoport:
	./autoport.pl 6 30000 31000 64

.include <bsd.regress.mk>
