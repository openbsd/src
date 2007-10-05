# $OpenBSD: Makefile,v 1.10 2007/10/05 16:58:21 otto Exp $

BC=bc

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18

# .in: input file
# .out: desired result

# t1: bc.library
# t2: while loop
# t3: for loop
# t4: deeply nested while loop
# t5: function definition and call
# t6: empty if statement
# t7: assignment ops
# t8: continue ops
# t9: more continue
# t10: for with empty E's
# t11: if else
# t12: print
# t13: alternive forms of define and return
# t14: compare
# t15: boolean and
# t16: boolean or
# t17: long var names
# t18: towers of hanoi

all: clean

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
