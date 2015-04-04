# $OpenBSD: Makefile,v 1.2 2015/04/04 18:52:12 schwarze Exp $

REGRESS_TARGETS	= broken broken_eline longarg noarg
LINT_TARGETS	= broken broken_eline noarg

# groff-1.22.3 defects:
# - .SS without args just before EOF causes two additional blank lines.
# - Empty .SS heads cause additional vertical spacing.
# - If .TP or .IP breaks .SS, the tag becomes bold.
# - If .HP breaks .SS, a line is broken after the next input line.
# - If .RS/RE/nf/fi breaks .SS, the next input line becomes bold.
# - .UR does not break .SS.

SKIP_GROFF	= broken broken_eline noarg

.include <bsd.regress.mk>
