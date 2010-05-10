# $OpenBSD: Makefile,v 1.7 2010/05/10 18:20:31 drahn Exp $

.include <bsd.own.mk>

NOMAN=
NOPROG=

.if ${COMPILER_VERSION:L} == "gcc3" 
TESTDIR=${BSDSRCDIR}/gnu/usr.bin/gcc
.elif ${COMPILER_VERSION:L} == "gcc4" 
TESTDIR=${BSDSRCDIR}/gnu/usr.bin/cc
.else
TESTDIR=${BSDSRCDIR}/gnu/egcs/gcc
.endif

REGRESS_TARGETS=do-gcc

.if !defined(DO_DEJATESTS)
REGRESS_SKIP_TARGETS=do-gcc
.endif

do-gcc: 
	@if [ ! "`pkg_info -e dejagnu-\*`" ]; then \
	    echo "Error: DejaGnu is required to run this test."; \
	    echo "  You can install the package from the ftp site or compile it"; \
	    echo "  from the ports tree (${PORTSDIR}/devel/dejagnu)."; \
	    exit 1; \
	fi
	@cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	${MAKE} check RUNTEST=${RUNTEST};

.include <bsd.regress.mk>

PORTSDIR?=/usr/ports
