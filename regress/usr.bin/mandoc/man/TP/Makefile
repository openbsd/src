# $OpenBSD: Makefile,v 1.14 2017/06/17 16:47:29 schwarze Exp $

REGRESS_TARGETS	 = badarg broken double eof fill indent literal longhead
REGRESS_TARGETS	+= macrotag manyargs sameline spacing width

LINT_TARGETS	 = broken double eof

# groff-1.22.3 defects:
# - If .TP precedes .RE, the latter does not properly reset indentation.
# - If the last line of the file is .TP, groff does not print a page footer.
# - A blank line in .TP next line scope causes a blank line.

SKIP_GROFF	 = broken eof

.include <bsd.regress.mk>
