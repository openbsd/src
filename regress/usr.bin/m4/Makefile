#	$OpenBSD: Makefile,v 1.9 2001/09/19 19:15:08 espie Exp $
#	$NetBSD: Makefile,v 1.2 1999/02/13 02:54:33 lukem Exp $

NOMAN=
NOPROG=

FIBOMAX=25
M4=m4

.SUFFIXES: .m4 .m4.uu
CLEANFILES+= ff_after_dnl.m4 strangequotes.m4 fibo.out

ALL_TESTS= test-ff_after_dnl test-m4wrap test-fibo test-patterns trip \
    test-strangequotes test-redef

regress: ${ALL_TESTS}

test-ff_after_dnl: ff_after_dnl.m4
	${M4} ff_after_dnl.m4 | diff - ${.CURDIR}/ff_after_dnl.out

.m4.uu.m4:
	uudecode $<

test-m4wrap:
	${M4} ${.CURDIR}/m4wrap.m4 | diff - ${.CURDIR}/m4wrap.out

test-patterns:
	${M4} ${.CURDIR}/patterns.m4 | diff - ${.CURDIR}/patterns.out

fibo.out:
	perl ${.CURDIR}/fibo.pl ${FIBOMAX} >$@

test-fibo: fibo.out
	${M4} -DN=${FIBOMAX} ${.CURDIR}/fibo.m4| diff - fibo.out

trip:
	echo "undefine(\`errprint')"|MALLOC_OPTIONS=A ${M4} -Derrprint=a >/dev/null

test-strangequotes: strangequotes.m4
	${M4} strangequotes.m4| diff - ${.CURDIR}/strangequotes.out

test-redef:
	${M4} ${.CURDIR}/redef.m4 | diff - ${.CURDIR}/redef.out

.PHONY:	${ALL_TESTS}

.include <bsd.prog.mk>
