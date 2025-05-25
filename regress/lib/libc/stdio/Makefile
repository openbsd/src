# $OpenBSD: Makefile,v 1.2 2025/05/25 00:20:54 yasuoka Exp $

PROGS=		test_fflush
CLEANFILES=	test_fflush.tmp

PROGS+=		test___freading
CLEANFILES+=	test___freading.tmp

PROGS+=		test___fwriting
CLEANFILES+=	test___fwriting.tmp

PROGS+=		test___fpending
CLEANFILES+=	test___fpending.tmp

PROGS+=		test___freadahead
CLEANFILES+=	test___freadahead.tmp

PROGS+=		test___freadptr
CLEANFILES+=	test___freadptr.tmp

PROGS+=		test___fseterr
CLEANFILES+=	test___fseterr.tmp

WARNINGS=	yes

# until fflush comply POSIX-2008
REGRESS_EXPECTED_FAILURES=	run-regress-test_fflush

.include <bsd.regress.mk>
