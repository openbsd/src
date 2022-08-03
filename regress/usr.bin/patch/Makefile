# $OpenBSD: Makefile,v 1.14 2022/08/03 07:35:04 op Exp $

PATCH?=		patch
PATCHFLAGS=	-sN
CLEANFILES=	*.copy *.orig *.rej t5 d19/*

REGRESS_TARGETS=     t1  t2  t3  t4  t5  t6  t7  t8  t9 \
		t10 t11 t12 t13 t14 t15 t16 t17 t18 t19 t20

# .in: input file
# .diff: patch
# .out: desired result after patching

# t1: diff contains invalid line number 0.
# t2: diff contains invalid line numbers beyond end of input file.
# t3: a case where patch should detect a previously applied patch.
#     Diff transform an empty file into a single line one.
# t4: a case where patch has to detect a previously applied patch.
#     Diff transform a file with a single line with an eol into a single
#     line without eol.
# t5: both files in diff do not exist. t5.in should be missing.
# t6-t12: various cases of no eol at end of file handling.
# t13: a case where patch has to detect a previously applied patch.
#     Diff transform a file ending with no eol into a file ending with eol.
# t14: diff in normal diff format.
# t15: diff in context diff format.
# t16: diff in ed format.
# t17: diff in ed format that inserts a dot-line.
# t18: diff in ed format that fully replaces input content.
# t19: git-produced unified diff.
# t20: reversal application of a patch to create a file with a single line.

.SUFFIXES: .in

.in:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@${PATCH} ${PATCHFLAGS} ${*}.copy ${.CURDIR}/${*}.diff
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t3:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHFLAGS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t4:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHFLAGS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t5:
	@echo ${*}
	@rm -f ${*}
	@${PATCH} ${PATCHFLAGS} < ${.CURDIR}/${*}.diff
	@cmp -s ${*} ${.CURDIR}/${*}.out || (echo "XXX ${*} failed" && false)

t13:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHFLAGS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t19:
	@echo t19
	@mkdir -p d19
	@cp ${.CURDIR}/t19.in d19/file
	@${PATCH} -t ${PATCHFLAGS} < ${.CURDIR}/t19.diff
	@cmp -s ${.CURDIR}/t19.out d19/file || (echo "XXX t19 failed" && false)

t20:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@${PATCH} ${PATCHFLAGS} -Rf ${*}.copy ${.CURDIR}/${*}.diff
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

.PHONY: t5

.include <bsd.regress.mk>
