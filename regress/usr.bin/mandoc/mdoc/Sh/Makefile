# $OpenBSD: Makefile,v 1.8 2017/03/05 19:59:26 schwarze Exp $

REGRESS_TARGETS	 = badNAME before empty emptyNAME first nohead order
REGRESS_TARGETS	+= orderNAME punctNAME subbefore
LINT_TARGETS	 = badNAME before empty emptyNAME first nohead order
LINT_TARGETS	+= orderNAME punctNAME subbefore
MARKDOWN_TARGETS  = badNAME before empty emptyNAME first nohead order
MARKDOWN_TARGETS += orderNAME punctNAME subbefore

SKIP_GROFF	= subbefore first empty
SKIP_ASCII	= first

.include <bsd.regress.mk>
