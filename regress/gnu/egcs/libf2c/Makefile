# $OpenBSD: Makefile,v 1.4 2004/07/21 04:28:49 david Exp $

.include <bsd.own.mk>

NOMAN=
NOPROG=


.if ${USE_GCC3:L:Myes}
TESTDIR=${BSDSRCDIR}/gnu/lib/libf2c
TEST2DIR=${BSDSRCDIR}/gnu/usr.bin/gcc
.else
TESTDIR=${BSDSRCDIR}/gnu/egcs/libf2c
TEST2DIR=${BSDSRCDIR}/gnu/egcs/gcc
.endif

#  We don't even try to run without objdir
TEST3DIR=${TESTDIR}/libU77
TEST4DIR=${TEST2DIR}/


regress: 
	@if ${RUNTEST} --help >/dev/null; then \
	    if ${RUNTEST} --version|grep 'Framework version is.*1.3$$' >/dev/null; then \
	    	echo "Error: you must have a dejagnu more recent than 1.3"; \
		echo "For instance: ${PORTSDIR}/devel/dejagnu."; \
	    else \
		cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
		cd ${TEST2DIR} && ${MAKE} -f Makefile.bsd-wrapper && \
		    cd ${TEST3DIR} && ${MAKE} G77DIR=${TEST4DIR} check RUNTEST=${RUNTEST}; \
	    fi; \
	else \
	    echo "Error: you must have a dejagnu more recent than 1.3 to run this test."; \
	    echo "  You can install the package from the ftp site or recompile it"; \
	    echo "  from the ports tree (${PORTSDIR}/devel/dejagnu)."; \
	fi


.include <bsd.prog.mk>

PORTSDIR?=/usr/ports
