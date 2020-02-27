# $OpenBSD: Makefile,v 1.13 2020/02/27 21:38:27 schwarze Exp $

REGRESS_TARGETS	 = badNAME before empty emptyNAME first nohead order
REGRESS_TARGETS	+= orderNAME paragraph parbefore parborder punctNAME
REGRESS_TARGETS	+= subbefore tag transp
LINT_TARGETS	 = badNAME before empty emptyNAME first nohead order
LINT_TARGETS	+= orderNAME parbefore parborder punctNAME subbefore
HTML_TARGETS	 = paragraph tag

# groff-1.22.3 defects:
# - .Pp before .Sh NAME causes a blank line before the header line
# - .Ss before .Sh NAME puts the subsection header before the header line
# - missing .Sh NAME causes loss of the header and footer lines
# - .Sh DESCRIPTION Xo aborts the parser

SKIP_GROFF	= parbefore subbefore first empty

.include <bsd.regress.mk>
