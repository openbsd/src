# $OpenBSD: Makefile,v 1.15 2025/07/27 15:38:55 schwarze Exp $

REGRESS_TARGETS	 = badNAME before empty emptyNAME first font nohead order
REGRESS_TARGETS	+= orderNAME paragraph parbefore parborder punctNAME
REGRESS_TARGETS	+= subbefore tag transp
LINT_TARGETS	 = badNAME before empty emptyNAME first nohead order
LINT_TARGETS	+= orderNAME parbefore parborder punctNAME subbefore tag
HTML_TARGETS	 = paragraph tag
TAG_TARGETS	 = tag
SKIP_MARKDOWN	 = font

# groff-1.22.3 defects:
# - .Pp before .Sh NAME causes a blank line before the header line
# - .Ss before .Sh NAME puts the subsection header before the header line
# - missing .Sh NAME causes loss of the header and footer lines
# - .Sh DESCRIPTION Xo aborts the parser

SKIP_GROFF	= parbefore subbefore first empty

.include <bsd.regress.mk>
