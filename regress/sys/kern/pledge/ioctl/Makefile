#	$OpenBSD: Makefile,v 1.2 2017/02/21 16:31:58 bluhm Exp $
TEST_CASES_OK+=		pfioctl1
TEST_CASES_FAILED+=	pfioctl2

REGRESS_TARGETS=	tests_ok tests_failed do-unfdpass1
WARNINGS=		Yes
CFLAGS+=		-Wall -Wundef -Werror
CLEANFILES+=		${TEST_CASES_OK} ${TEST_CASES_FAILED} *.core \
			file1 file2 file3 output test-sock unfdpass

tests_ok: ${TEST_CASES_OK}
	@for test in $>; do \
		${SUDO} ./$$test; \
	done

tests_failed: ${TEST_CASES_FAILED}
	@for test in $>; do \
		rm -f ./$$test.core; \
		if ${SUDO} ./$$test; then false; else true; fi; \
		rm -f ./$$test.core; \
	done

do-unfdpass1: unfdpass
	${SUDO} ./unfdpass > output
	cmp -s ${.CURDIR}/expected output
#	${SUDO} ./unfdpass -f > output
#	cmp -s ${.CURDIR}/expectedfail output

.include <bsd.regress.mk>
