# $OpenBSD: Makefile,v 1.16 2020/02/27 01:25:58 schwarze Exp $

REGRESS_TARGETS	 = badarg broken double eof fill indent literal longhead
REGRESS_TARGETS	+= macrotag manyargs sameline spacing vert width
LINT_TARGETS	 = broken double eof
HTML_TARGETS	 = literal vert

# groff-1.22.3 defects:
# - If .TP precedes .RE, the latter does not properly reset indentation.
# - If the last line of the file is .TP, groff does not print a page footer.
# - A blank line in .TP next line scope causes a blank line.

SKIP_GROFF	 = broken eof

.include <bsd.regress.mk>
