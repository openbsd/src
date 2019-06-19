# $OpenBSD: Makefile,v 1.2 2019/04/21 22:43:00 schwarze Exp $

REGRESS_TARGETS	= basic badargs break into nesting outof
LINT_TARGETS	= badargs into nesting outof

# mandoc defects:
#  - if a while loop extends into a scope, mandoc may close it there
#  - mandoc does not support nested .while loops
#  - mandoc does not support .while loops extending out of the current scope

SKIP_GROFF	= into nesting outof

.include <bsd.regress.mk>
