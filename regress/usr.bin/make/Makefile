# $OpenBSD: Makefile,v 1.4 2000/07/24 21:53:27 espie Exp $

NOMAN=
NOPROG=

regress: t1 t2 t3 t4 t5 t6 t7 t8 t9

t1: t1.out
	env -i PATH=${PATH} ${MAKE} -e -r -f ${.CURDIR}/mk1 | diff - t1.out


# This is a POSIX test. pmake does not pass variables to submakes until
# after OpenBSD 2.7.
t2:
	cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mk2| diff - t2.out

t3:
	cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mk3 -f mk3a|diff - t3.out
	cd ${.CURDIR} && cat mk3a |env -i PATH=${PATH} ${MAKE} -r -f mk3 -f -|diff - t3.out
	cd ${.CURDIR} && cat mk3 |env -i PATH=${PATH} ${MAKE} -r -f - -f mk3a|diff - t3.out
	cd ${.CURDIR} && cat mk3 mk3a|env -i PATH=${PATH} ${MAKE} -r -f - |diff - t3.out

t4:
	cd ${.CURDIR} && cat mk4 mk4a|env -i PATH=${PATH} ${MAKE} -r -f - |diff - t4.out
	# Splitting files along conditionals should not work
	if cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mk4 -f mk4a 2>/dev/null; then false; fi

# Another POSIX test, checking that MAKEFLAGS does work
t5:
	# first check that make does fail as expected
	if cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mkfail 2>/dev/null; then false; fi
	# first form of MAKEFLAGS
	cd ${.CURDIR} && env -i PATH=${PATH} MAKEFLAGS=-n ${MAKE} -r -f mkfail
	# second form of MAKEFLAGS
	cd ${.CURDIR} && env -i PATH=${PATH} MAKEFLAGS=n ${MAKE} -r -f mkfail

t6:
	cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} A=good -r -f mk6 | diff - t6.out

t7:
	if cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mk7 2>/dev/null; then false; fi

# check for existent/non existent files
t8:
	touch goodfile phony
	cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mk8 | diff - t8.out

t9:
	cd ${.CURDIR} && env -i PATH=${PATH} ${MAKE} -r -f mk9 2>&1 | diff - t9.out
	
t1.out:
	echo MACHINE_ARCH=${MACHINE_ARCH} >$@

.PHONY: t1 t2 t3 t4 t5 t6 t7 t8 t9 regress

CLEANFILES+=t1.out

.include <bsd.prog.mk>
