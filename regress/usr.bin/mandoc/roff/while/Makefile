# $OpenBSD: Makefile,v 1.1 2018/08/24 22:56:37 schwarze Exp $

REGRESS_TARGETS	= basic badargs into nesting outof
LINT_TARGETS	= badargs into nesting outof

# mandoc defects:
#  - if a while loop extends into a scope, mandoc may close it there
#  - mandoc does not support nested .while loops
#  - mandoc does not support .while loops extending out of the current scope

SKIP_GROFF	= into nesting outof

.include <bsd.regress.mk>
