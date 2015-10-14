#	$OpenBSD: Makefile,v 1.2 2015/10/14 13:23:25 vgross Exp $

REGRESS_TARGETS=	run-regress-autoport
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-autoport:
	/usr/bin/perl ${.CURDIR}/autoport.pl 6 30000 31000 64

.include <bsd.regress.mk>
