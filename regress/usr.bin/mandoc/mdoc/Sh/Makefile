# $OpenBSD: Makefile,v 1.6 2015/02/06 01:07:07 schwarze Exp $

REGRESS_TARGETS	 = badNAME before empty emptyNAME first nohead order subbefore
LINT_TARGETS	 = badNAME before empty emptyNAME first nohead order subbefore

SKIP_GROFF	= subbefore first empty
SKIP_ASCII	= first

.include <bsd.regress.mk>
