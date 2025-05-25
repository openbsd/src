# $OpenBSD: Makefile,v 1.3 2025/05/25 05:32:45 yasuoka Exp $

PROGS=		test_fflush
CLEANFILES=	test_fflush.tmp

PROGS+=		test_ungetwc
CLEANFILES+=	test_ungetwc.tmp

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
