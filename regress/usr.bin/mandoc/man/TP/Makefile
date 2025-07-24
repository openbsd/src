# $OpenBSD: Makefile,v 1.18 2025/07/24 10:55:09 schwarze Exp $

REGRESS_TARGETS	 = badarg broken double eof fill fraction indent
REGRESS_TARGETS	+= literal longhead macrotag manyargs
REGRESS_TARGETS	+= sameline spacing tag vert width
TAG_TARGETS	 = tag
LINT_TARGETS	 = broken double eof
HTML_TARGETS	 = literal tag vert

# groff-1.23.0 defects:
# - If .TP precedes .RE, the latter does not properly reset indentation.
# - If the last line of the file is .TP, groff does not print a page footer.
# - A blank line in .TP next line scope causes a blank line.

SKIP_GROFF	 = broken eof

.include <bsd.regress.mk>
