# $OpenBSD: Makefile,v 1.1 2001/10/07 13:24:56 espie Exp $

NOMAN=
NOPROG=


TESTDIR=${BSDSRCDIR}/gnu/egcs/libf2c
TEST2DIR=${BSDSRCDIR}/gnu/egcs/gcc

#  We don't even try to run without objdir
TEST3DIR=${BSDOBJDIR}/gnu/egcs/libf2c/libU77
TEST4DIR=${BSDOBJDIR}/gnu/egcs/gcc/


regress: 
	@if ${RUNTEST} --help >/dev/null; then \
	    if ${RUNTEST} --version|grep 'Framework version is.*1.3$$' >/dev/null; then \
	    	echo "Error: you must have a dejagnu more recent than 1.3"; \
		echo "For instance: ${PORTSDIR}/lang/egcs/dejagnu."; \
	    else \
		cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
		cd ${TEST2DIR} && ${MAKE} -f Makefile.bsd-wrapper && \
		    cd ${TEST3DIR} && ${MAKE} G77DIR=${TEST4DIR} check RUNTEST=${RUNTEST}; \
	    fi; \
	else \
	    echo "Error: you must have a dejagnu more recent than 1.3 to run this test."; \
	    echo "  You can install the package from the ftp site or recompile it"; \
	    echo "  from the ports tree (${PORTSDIR}/lang/egcs/dejagnu)."; \
	fi


.include <bsd.prog.mk>

PORTSDIR?=/usr/ports
