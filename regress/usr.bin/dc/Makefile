# $OpenBSD: Makefile,v 1.13 2004/12/01 08:30:53 otto Exp $

DC=dc -x

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 \
	t18 t19 t20 t21 t22

# .in: input file
# .out: desired result

# t1: basic operations, prints "dc: Runtime warning: non-zero scale in exponent"
# t2: factoring program
# t3: string handling
# t4: more string ops
# t5: bc output, computing exp
# t6: nesting level and tail recursion
# t7: number input and string handling
# t8: bc output, array operations
# t9: output for different bases and wrapping of long lines
# t10: test J and M operators
# t11: test extended comparison operators
# t12: test r operator, prints two "stack empty" warnings
# t13: test #, n and a
# t14: test N, G, ( and {
# t15: more N, G, ( and {
# t16: even more N, G, ( and {
# t17: test R (drop)
# t18: read uninitialized and initialized array locations
# t19: test extended registers
# t20: towers of hanoi
# t21: 500 decimals of pi
# t22: sqrt

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
