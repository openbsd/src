# $OpenBSD: Makefile,v 1.1 2003/09/25 19:40:07 otto Exp $

BC=bc

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6

# .in: input file
# .out: desired result

# t1: bc.library
# t2: while loop
# t3: for loop
# t4: deep nested while loop
# t5: function definition and call
# t6: empty if statement

all: clean ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@echo ${*}
	@${BC} -d < ${.CURDIR}/${*}.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

# Clean all files generated
clean:
	rm -f *.log

.include <bsd.regress.mk>
