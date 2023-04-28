# $OpenBSD: Makefile,v 1.2 2023/04/28 19:41:07 bluhm Exp $

# evaluate once in main make invocation, pass test list in environment
.if ! (make(clean) || make(cleandir) || make(obj))
RSYNC_TESTS ?!=	ls -1 ${.CURDIR}/*.test
.MAKEFLAGS :=	RSYNC_TESTS="${RSYNC_TESTS}"
.endif

clean:
	rm -rf dir* find*

# these fail permanently and should be investigated
REGRESS_EXPECTED_FAILURES =	run-test11_middlediff \
				run-test11b_middlediff \
				run-test3_minusexclude \
				run-test7_symlinks
# these fail randomly with Permission denied
#				run-test6_perms
#				run-test6b_perms

.for t in ${RSYNC_TESTS:T:R}
REGRESS_TARGETS +=	run-$t
run-$t:
	tstdir=${.CURDIR} sh ${.CURDIR}/$t.test
.endfor

.include <bsd.regress.mk>
