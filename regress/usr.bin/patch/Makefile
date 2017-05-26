# $OpenBSD: Makefile,v 1.12 2017/05/26 20:27:21 tedu Exp $

PATCH=patch
PATCHOPTIONS=-sN

REGRESS_TARGETS=t1 t2 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18 t19

# Currently t3 fails

# .in: input file
# .diff: patch
# .out: desired result after patching

# t1: diff contains invalid line number 0.
# t2: diff contains invalid line numbers beyond end of input file.
# t3: a case where patch should detect a previously applied patch.
#     Diff transform an empty file into a single line one. Currently fails.
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

all: clean ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@${PATCH} ${PATCHOPTIONS} ${*}.copy ${.CURDIR}/${*}.diff
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t3:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHOPTIONS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)
t4:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHOPTIONS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t5:
	@echo ${*}
	@rm -f ${*}
	@${PATCH} ${PATCHOPTIONS} < ${.CURDIR}/${*}.diff
	@cmp -s ${*} ${.CURDIR}/${*}.out || (echo "XXX ${*} failed" && false)

t13:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHOPTIONS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || \
		(echo "XXX ${*} failed" && false)

t19:
	@echo t19
	@mkdir -p d19
	@cp ${.CURDIR}/t19.in d19/file
	@${PATCH} -t ${PATCHOPTIONS} < ${.CURDIR}/t19.diff
	@cmp -s ${.CURDIR}/t19.out d19/file || (echo "XXX t19 failed" && false)


# Clean all files generated
clean:
	rm -rf *.copy *.orig *.rej t5 d19

.PHONY: t5

.include <bsd.regress.mk>
