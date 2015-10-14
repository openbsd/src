#	$OpenBSD: Makefile,v 1.1 2015/10/14 12:47:07 vgross Exp $

DIR=	../../netinet6/autoport

REGRESS_TARGETS=	run-regress-autoport
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-autoport:
	$(DIR)/autoport.pl 4 30000 31000 64

.include <bsd.regress.mk>
