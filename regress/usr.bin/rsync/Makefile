# $OpenBSD: Makefile,v 1.3 2023/04/29 00:20:46 bluhm Exp $

# evaluate once in main make invocation, then get list of test from environment
.if ! (make(clean) || make(cleandir) || make(obj))
RSYNC_TESTS ?!=	ls -1 ${.CURDIR}/*.test
.MAKEFLAGS :=	RSYNC_TESTS="${RSYNC_TESTS}"
.endif

RSYNC_OPEN =	/usr/bin/openrsync
RSYNC_PORT =	/usr/local/bin/rsync

clean:
	rm -rf dir* find*

# some of these tests pass or fail unreliably
REGRESS_EXPECTED_FAILURES =	run-openrsync-openrsync-test10_perms \
				run-openrsync-openrsync-test11_middlediff \
				run-openrsync-openrsync-test11b_middlediff \
				run-openrsync-openrsync-test3_minusexclude \
				run-openrsync-openrsync-test3b_minusexclude \
				run-openrsync-openrsync-test3c_minusexclude \
				run-openrsync-openrsync-test6_perms \
				run-openrsync-openrsync-test6b_perms \
				run-openrsync-openrsync-test7_symlinks \
				run-openrsync-openrsync-test7b_symlinks \
				run-openrsync-openrsync-test9_norecurse \
				run-openrsync-rsync-test10_perms \
				run-openrsync-rsync-test11_middlediff \
				run-openrsync-rsync-test11b_middlediff \
				run-openrsync-rsync-test3_minusexclude \
				run-openrsync-rsync-test6_perms \
				run-openrsync-rsync-test6b_perms \
				run-openrsync-rsync-test7_symlinks \
				run-rsync-openrsync-test12_inex \
				run-openrsync-rsync-test12c_inex \
				run-rsync-rsync-test3c_minusexclude

.if ! exists(${RSYNC_PORT})
.for t in ${RSYNC_TESTS}
REGRESS_SKIP_TARGETS +=		run-${RSYNC_OPEN:T}-${RSYNC_PORT:T}-${t:T:R}
REGRESS_SKIP_TARGETS +=		run-${RSYNC_PORT:T}-${RSYNC_OPEN:T}-${t:T:R}
REGRESS_SKIP_TARGETS +=		run-${RSYNC_PORT:T}-${RSYNC_PORT:T}-${t:T:R}
.endfor
.endif

.for l in ${RSYNC_OPEN} ${RSYNC_PORT}
.for r in ${RSYNC_OPEN} ${RSYNC_PORT}

RSYNC_$l_$r =	$l --rsync-path=$r

.for t in ${RSYNC_TESTS}
REGRESS_TARGETS +=		run-${l:T}-${r:T}-${t:T:R}
run-${l:T}-${r:T}-${t:T:R}:
	tstdir="${.CURDIR}" rsync="${RSYNC_$l_$l}" sh $t
.endfor

.endfor
.endfor

.include <bsd.regress.mk>
