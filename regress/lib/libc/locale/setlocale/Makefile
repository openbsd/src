#	$OpenBSD: Makefile,v 1.1 2015/07/07 17:38:53 semarie Exp $

NOMAN =
PROG =	setlocale

run-regress-setlocale: ${PROG}
	env -i LC_ALL=fr_FR.UTF-8 ./${PROG}

.include <bsd.regress.mk>
