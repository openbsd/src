# $OpenBSD: Makefile,v 1.7 2017/01/08 00:10:22 schwarze Exp $

REGRESS_TARGETS	 = badNAME before empty emptyNAME first nohead order
REGRESS_TARGETS	+= orderNAME punctNAME subbefore
LINT_TARGETS	 = badNAME before empty emptyNAME first nohead order
LINT_TARGETS	+= orderNAME punctNAME subbefore

SKIP_GROFF	= subbefore first empty
SKIP_ASCII	= first

.include <bsd.regress.mk>
