# $OpenBSD: Makefile,v 1.19 2017/03/08 14:45:33 schwarze Exp $

REGRESS_TARGETS	 = beforeNAME blank centered nested spacing
REGRESS_TARGETS	+= badargs empty offset-empty offset-neg
REGRESS_TARGETS	+= break broken unclosed

LINT_TARGETS	 = beforeNAME blank nested badargs break broken unclosed

MARKDOWN_TARGETS  = beforeNAME blank centered nested spacing
MARKDOWN_TARGETS += badargs empty offset-empty offset-neg
MARKDOWN_TARGETS += break broken unclosed

SKIP_TMAN	 = beforeNAME centered

# groff-1.22.3 defects:
# - a display breaking another block continues indefinitely
# - negative offsets take no effect, but advance the indention on exit

SKIP_GROFF	 = break offset-neg

# adjustment is disabled in groff for now

SKIP_GROFF	+= centered

# groff handles content before NAME differently

SKIP_GROFF	+= beforeNAME

.include <bsd.regress.mk>
