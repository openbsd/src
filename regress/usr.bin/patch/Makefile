# $OpenBSD: Makefile,v 1.2 2003/08/07 21:01:29 otto Exp $

PATCH=patch
PATCHOPTIONS=-sN

REGRESS_TARGETS=t1 t2 t3 t4

# .in: input file
# .diff: patch
# .out: desired result after patching

# t1: diff contains invalid line number 0.
# t2: diff contains invalid line numbers beyond end of input file.
# t3: a case where it patch should detect a previously applied patch.
#     Diff transform an empty file into a single line one. Patch cannot
#     detect this because it has an empty context.
# t4: a case where patch has to detect a previously applied patch.
#     Diff transform an file with a single line without eol into a single
#     line with eol. Fails because patch does not detect the previously
#     applied diff.

all: clean ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@${PATCH} ${PATCHOPTIONS} ${*}.copy ${.CURDIR}/${*}.diff
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || (echo "XXX ${*} failed" && false)

t4:
	@echo ${*}
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@(! ${PATCH} ${PATCHOPTIONS} ${*}.copy ${.CURDIR}/${*}.diff)
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || (echo "XXX ${*} failed" && false)
	
# Clean all files generated
clean:
	rm -f *.copy *.orig *.rej

.include <bsd.regress.mk>
