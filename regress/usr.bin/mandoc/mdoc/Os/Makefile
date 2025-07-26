# $OpenBSD: Makefile,v 1.5 2025/07/26 14:15:29 schwarze Exp $

REGRESS_TARGETS	 = dupe late long missing
LINT_TARGETS	 = dupe late missing

# groff-1.23.0 defect:
#  - footer fields of excessive length get printed on top of each other
#    rather than breaking the output line.

SKIP_GROFF	 = long

# groff-1.23.0 regressions:
# One header line is printed for every .Os macro, wherever that macro occurs.
#  - If there is no .Os macro at all, no header is printed anywhere.
#  - If an .Os macro comes late, a header is printed in the middle
#    of the page rather than at the top.
#  - If the .Os macros precedes all .Dt macros, the header
#    reads "UNTITLED   LOCAL   UNTITLED".
#  - If there are multiple .Os macros, multiple headers are printed.

SKIP_GROFF	+= dupe late missing

.include <bsd.regress.mk>
