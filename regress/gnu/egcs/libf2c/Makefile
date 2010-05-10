# $OpenBSD: Makefile,v 1.7 2010/05/10 18:20:31 drahn Exp $

.include <bsd.own.mk>

NOMAN=
NOPROG=

.if ${COMPILER_VERSION:L} == "gcc3" 
TESTDIR=${BSDSRCDIR}/gnu/lib/libf2c
TEST2DIR=${BSDSRCDIR}/gnu/usr.bin/gcc
.elif ${COMPILER_VERSION:L} == "gcc2" 
TESTDIR=${BSDSRCDIR}/gnu/egcs/libf2c
TEST2DIR=${BSDSRCDIR}/gnu/egcs/gcc
.endif

#  We don't even try to run without objdir
TEST3DIR=${TESTDIR}/libU77
TEST4DIR=${TEST2DIR}/

REGRESS_TARGETS=do-libf2c

.if !defined(DO_DEJATESTS) && defined(TESTDIR)
REGRESS_SKIP_TARGETS=do-libf2c
.endif

do-libf2c: 
	@if [ ! "`pkg_info -e dejagnu-\*`" ]; then \
	    echo "Error: DejaGnu is required to run this test."; \
	    echo "  You can install the package from the ftp site or compile it"; \
	    echo "  from the ports tree (${PORTSDIR}/devel/dejagnu)."; \
	    exit 1; \
	fi
	@cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	cd ${TEST2DIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	cd ${TEST3DIR} && ${MAKE} G77DIR=${TEST4DIR} check RUNTEST=${RUNTEST};

.include <bsd.regress.mk>

PORTSDIR?=/usr/ports
