# $OpenBSD: Makefile,v 1.1 2003/07/22 19:31:51 otto Exp $

PATCH=patch

REGRESS_TARGETS=t1 t2

# .in: input file
# .diff: patch
# .out: desired result after patching

# t1: diff contains invalid line number 0. Fails.
# t2: diff contains invalid line numbers beyond end of input file.

all: clean ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@cp ${.CURDIR}/${*}.in ${*}.copy
	@${PATCH} -s ${*}.copy ${.CURDIR}/${*}.diff
	@cmp -s ${*}.copy ${.CURDIR}/${*}.out || (echo "XXX ${*} failed" && false)

# Clean all files generated
clean:
	rm -f *.copy *.orig *.rej

.include <bsd.regress.mk>
