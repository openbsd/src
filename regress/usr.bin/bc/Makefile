# $OpenBSD: Makefile,v 1.6 2003/11/11 19:50:50 otto Exp $

BC=bc

REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13

# .in: input file
# .out: desired result

# t1: bc.library
# t2: while loop
# t3: for loop
# t4: deep nested while loop
# t5: function definition and call
# t6: empty if statement
# t7: assignment ops
# t8: continue ops
# t9: more continue
# t10: for with empty E's
# t11: if else
# t12: print
# t13: alternive forms of define and return

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
