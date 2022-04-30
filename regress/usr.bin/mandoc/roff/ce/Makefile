# $OpenBSD: Makefile,v 1.2 2022/04/30 18:46:16 schwarze Exp $

REGRESS_TARGETS	 = basic break

# mandoc defect:
# - For now, high level macros break .ce and .rj blocks.

SKIP_GROFF	 = break

.include <bsd.regress.mk>
