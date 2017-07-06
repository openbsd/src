# $OpenBSD: Makefile,v 1.8 2017/07/06 21:33:45 bluhm Exp $

DIFF?=		diff
PATCH?=		patch
PATCHFLAGS=	-sb
CLEANFILES=	*.copy *.patch *.orig

REGRESS_TARGETS=     t1  t2  t3  t4  t5  t6  t7  t8  t9 \
		t10 t11 t12 t13 t14 t15

# Skip the cmp(1) part for ed(1) style diffs for these tests
EDSKIPCMP=t7 t10 t12

# .1 and .2: 	input files

# t1: two empty files
# t2: basic editing
# t3: one empty, other contains text
# t4: one contains text, other empty
# t5: files are the same
# t6: one file with no newline at end, other does contain newline
# t7: one file with newline at end, the other not
# t8: revision 1.1 and 1.54 of kern_malloc.c
# t9: revision 1.1 and 1.104 of vfs_syscalls.c
# t10: two files with no end of line at the end, line in second file is longer
# t11: rev 1.3 and 1.36 of usr.bin/ed/ed.1.
# t12: rev 1.1 and 1.2 of sbin/isakmpd/regress/hmac/Makefile.
# t13: a case to check the single dot on a line handling for ed(1) patches.
# t14: a case to ensure unified diffs ignoring whitespace produce valid output
#      when extra lines and no newline at end of file1
# t15: a case to ensure unified diffs ignoring whitespace produce valid output
#      when fewer lines and no newline at end of file1

.SUFFIXES: .1 .2

.1:
	@cp ${.CURDIR}/${*}.1 ${*}.copy
	@cp ${.CURDIR}/${*}.1 ${*}.c.copy
	@cp ${.CURDIR}/${*}.1 ${*}.u.copy
	@cp ${.CURDIR}/${*}.1 ${*}.e.copy
	@${DIFF}  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.patch || true
	@${DIFF} -c  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.c.patch || true
	@${DIFF} -u  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.u.patch || true
	@${DIFF} -e  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.e.patch || true
	@${PATCH} ${PATCHFLAGS} ${*}.copy ${*}.patch || true
	@${PATCH} ${PATCHFLAGS} ${*}.c.copy ${*}.c.patch || true
	@${PATCH} ${PATCHFLAGS} ${*}.u.copy ${*}.u.patch || true
	@${PATCH} ${PATCHFLAGS} ${*}.e.copy ${*}.e.patch || true
	@cmp -s ${.CURDIR}/${*}.2 ${*}.copy || \
		(echo "XXX ${*} standard diff failed" && false)
	@cmp -s ${.CURDIR}/${*}.2 ${*}.c.copy || \
		(echo "XXX ${*} context diff failed" && false)
	@cmp -s ${.CURDIR}/${*}.2 ${*}.u.copy || \
		(echo "XXX ${*} unified diff failed" && false)
	@( echo ${EDSKIPCMP} | grep -q '[[:<:]]${*}[[:>:]]' ) || \
		cmp -s ${.CURDIR}/${*}.2 ${*}.e.copy || \
		(echo "XXX ${*} ed diff failed" && false)

t14.1 t15.1:
	@cp ${.CURDIR}/${*}.1 ${*}.uw.copy
	@${DIFF} -uw  ${.CURDIR}/${*}.1 ${.CURDIR}/${*}.2 > ${*}.uw.patch || true
	@${PATCH} ${PATCHFLAGS} ${*}.uw.copy ${*}.uw.patch || true
	@cmp -s ${.CURDIR}/${*}.2 ${*}.uw.copy || \
		(echo "XXX ${*} unified diff ignoring whitespace failed" && false)

.include <bsd.regress.mk>
