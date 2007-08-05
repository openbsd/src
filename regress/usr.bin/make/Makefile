# $OpenBSD: Makefile,v 1.25 2007/08/05 11:10:11 espie Exp $

# We don't pass t7, t13, t14, t17, t19, t20 t21 t26
# and t16 yields piss poor performance
REGRESS_TARGETS=t1 t2 t3 t4 t5 t6 t7 t8 t9 t10 t11 t12 t13 t14 t15 t16 t17 t18 t19 t20 t21 t22 t23 t24 t25 t26 t27 t28

MALLOC_OPTIONS?=AZJ
t1: t1.out
	env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -e -r -f ${.CURDIR}/mk1 | diff - t1.out


# This is a POSIX test. pmake does not pass variables to submakes until
# after OpenBSD 2.7.
t2:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk2| diff - t2.out

t3:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk3 -f mk3a|diff - t3.out
	cd ${.CURDIR} && cat mk3a |env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk3 -f -|diff - t3.out
	cd ${.CURDIR} && cat mk3 |env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f - -f mk3a|diff - t3.out
	cd ${.CURDIR} && cat mk3 mk3a|env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f - |diff - t3.out

t4:
	cd ${.CURDIR} && cat mk4 mk4a|env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f - |diff - t4.out
	# Splitting files along conditionals should not work
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk4 -f mk4a 2>/dev/null; then false; fi

# Another POSIX test, checking that MAKEFLAGS does work
t5:
	# first check that make does fail as expected
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mkfail 2>/dev/null; then false; fi
	# first form of MAKEFLAGS
	cd ${.CURDIR} && env -i PATH=${PATH} MAKEFLAGS=-n MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mkfail
	# second form of MAKEFLAGS
	cd ${.CURDIR} && env -i PATH=${PATH} MAKEFLAGS=n MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mkfail

t6:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} A=good -r -f mk6 | diff - t6.out

# This test currently fails, and will until the cond parser is rewritten.
t7:
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk7 2>/dev/null; then false; fi

# check for existent/non existent files
t8:
	touch goodfile phony
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk8 | diff - t8.out

t9:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk9 2>&1 | diff - t9.out

# verify that systemV extended variables work as expected
t10:	z.a
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk10 |diff - t10.out
	
# check for substitution parser in some confusing situations
t11:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk11 |diff - t11.out
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk11 A=t|diff - t11.out2

# Check for filename expansions.
t12:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk12 |diff - t12.out

# Verify whitespace handling. Currently failing.
t13:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk13 |diff - t13.out

t14:
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk14 2>/dev/null; then false; fi

t15:
	touch t15file
	cd ${.CURDIR} && ulimit -t 2 && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk15 2>/dev/null

t16:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk16

t17:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk17

t18:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk18

t19:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} SHELL=/bin/zsh ${MAKE} -r -f mk19 | diff - t19.out

t20:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk20 | diff - t20.out

t21:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk21 | diff - t21.out

t22:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk22 b | diff - t22.out

t23:
	cd ${.CURDIR} && ulimit -t 60 && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk22 a 2>&1| diff - t23.out

t24:
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk23 a 2>/dev/null; then false; fi

t25:
	touch ${.OBJDIR}/t25.1st
	rm -f ${.OBJDIR}/t25.2nd
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk24 t25.2nd 2>/dev/null; then false; fi

t26:
	touch ${.OBJDIR}/f26.2nd
	rm -f ${.OBJDIR}/f26
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk24 f26 2>/dev/null; then false; fi

t27:
	touch ${.OBJDIR}/t27.2nd
	rm -f ${.OBJDIR}/t27.3rd
	if cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk24 t27.3rd 2>/dev/null; then false; fi

t28:
	cd ${.CURDIR} && env -i PATH=${PATH} MALLOC_OPTIONS=${MALLOC_OPTIONS} ${MAKE} -r -f mk25 all >/dev/null
	
z.a:
	touch $@

t1.out:
	echo MACHINE_ARCH=${MACHINE_ARCH} >$@

.PHONY: ${REGRESS_TARGETS} regress

CLEANFILES+=t1.out z.a a.a a.b goodfile phony t15file \
	t25.1st t25.2nd f26.2nd f26 t27.2nd t27.3rd

.include <bsd.regress.mk>
