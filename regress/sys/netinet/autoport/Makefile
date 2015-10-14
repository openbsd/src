#	$OpenBSD: Makefile,v 1.2 2015/10/14 13:17:33 vgross Exp $

DIR=	${.CURDIR}/../../netinet6/autoport

REGRESS_TARGETS=	run-regress-autoport
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-autoport:
	/usr/bin/perl $(DIR)/autoport.pl 4 30000 31000 64

.include <bsd.regress.mk>
