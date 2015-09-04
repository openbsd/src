# $OpenBSD: Makefile,v 1.11 2015/09/04 21:24:26 schwarze Exp $

REGRESS_TARGETS	 = badarg broken double eof fill literal longhead
REGRESS_TARGETS	+= macrotag manyargs sameline width

LINT_TARGETS	 = broken double eof

# groff-1.22.3 defects:
# - If .TP precedes .RE, the latter does not properly reset indentation.
# - If the last line of the file is .TP, groff does not print a page footer.

SKIP_GROFF	 = broken eof

.include <bsd.regress.mk>
