# $OpenBSD: Makefile,v 1.1 2025/05/24 11:07:20 yasuoka Exp $

PROGS=		test_fflush

CLEANFILES=	test_fflush.tmp
WARNINGS=	yes

# until fflush comply POSIX-2008
REGRESS_EXPECTED_FAILURES=	run-regress-test_fflush

.include <bsd.regress.mk>
