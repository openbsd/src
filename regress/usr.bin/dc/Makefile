# $OpenBSD: Makefile,v 1.4 2003/11/04 08:11:41 otto Exp $

DC=dc

REGRESS_TARGETS=t1 t2 t4 t5 t6 t7 t8 t9 t10 t11 t12

# .in: input file
# .out: desired result

# t1: basic operations, prints "dc: Runtime warning: non-zero scale in exponent"
# t2: factoring program
# t4: more string ops
# t5: bc output, computing exp
# t6: nesting level and tail recursion
# t7: number input and string handling
# t8: bc output, array operations
# t9: output for different bases and wrapping of long lines
# t10: test J and M operators
# t11: test extended comparison operators
# t12: test r operator, prints two "stack empty" warnings

all: clean ${REGRESS_TARGET}

.SUFFIXES: .in

.in:
	@echo ${*}
	@${DC} ${.CURDIR}/${*}.in > ${*}.log
	@cmp -s ${.CURDIR}/${*}.out ${*}.log || \
		(echo "XXX ${*} failed" && false)

# Clean all files generated
clean:
	rm -f *.log

.include <bsd.regress.mk>
