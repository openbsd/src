#	$OpenBSD: Makefile,v 1.1 2015/10/26 08:04:10 vgross Exp $

DIR=	${.CURDIR}/../../net/rdomains

REGRESS_TARGETS=	run-regress-ifalocal
REGRESS_ROOT_TARGETS=	$(REGRESS_TARGETS)

run-regress-ifalocal:
	sh $(DIR)/ifalocal.sh

.include <bsd.regress.mk>
