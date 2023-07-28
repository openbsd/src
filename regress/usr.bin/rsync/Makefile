# $OpenBSD: Makefile,v 1.5 2023/07/28 17:25:32 bluhm Exp $

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
REGRESS_EXPECTED_FAILURES =	run-openrsync-openrsync-test3_minusexclude \
				run-openrsync-openrsync-test6b_perms \
				run-openrsync-openrsync-test7_symlinks \
				run-openrsync-openrsync-test10_perms \
				run-openrsync-openrsync-test13b_perms \
				run-openrsync-rsync-test3_minusexclude \
				run-openrsync-rsync-test6b_perms \
				run-openrsync-rsync-test7_symlinks \
				run-openrsync-rsync-test10_perms \
				run-openrsync-rsync-test13b_perms

.if ! exists(${RSYNC_PORT})
.for t in ${RSYNC_TESTS}
REGRESS_SKIP_TARGETS +=		run-${RSYNC_OPEN:T}-${RSYNC_PORT:T}-${t:T:R}
REGRESS_SKIP_TARGETS +=		run-${RSYNC_PORT:T}-${RSYNC_OPEN:T}-${t:T:R}
REGRESS_SKIP_TARGETS +=		run-${RSYNC_PORT:T}-${RSYNC_PORT:T}-${t:T:R}
.endfor
.endif

# these test have to run as regular user
USER_TESTS =			test13b_perms

ROOT_USER !=			id -g
.for t in ${RSYNC_TESTS}
.if ${ROOT_USER} != 0 || empty(USER_TESTS:M${t:T:R})
SH_${t:T:R} =			sh
.else
SH_${t:T:R} =			su build
.endif
.endfor

.for l in ${RSYNC_OPEN} ${RSYNC_PORT}
.for r in ${RSYNC_OPEN} ${RSYNC_PORT}

RSYNC_$l_$r =	$l --rsync-path=$r

.for t in ${RSYNC_TESTS}
REGRESS_TARGETS +=		run-${l:T}-${r:T}-${t:T:R}
run-${l:T}-${r:T}-${t:T:R}: clean
	tstdir="${.CURDIR}" rsync="${RSYNC_$l_$l}" ${SH_${t:T:R}} $t
.endfor

.endfor
.endfor

.include <bsd.regress.mk>
