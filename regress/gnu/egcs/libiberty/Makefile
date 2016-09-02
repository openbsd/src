# $OpenBSD: Makefile,v 1.6 2016/09/02 21:30:34 bluhm Exp $

NOMAN=
NOPROG=

TESTDIR=${BSDSRCDIR}/gnu/lib/libiberty

REGRESS_TARGETS=do-libiberty

.if !defined(DO_DEJATESTS)
REGRESS_SKIP_TARGETS=do-libiberty
.endif

do-libiberty: 
	@if [ ! "`pkg_info -e dejagnu-\*`" ]; then \
	    echo "Error: DejaGnu is required to run this test."; \
	    echo "  You can install the package from the ftp site or compile it"; \
	    echo "  from the ports tree (${PORTSDIR}/devel/dejagnu)."; \
	    echo SKIPPED; \
	    exit 1; \
	fi
	@cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	${MAKE} check RUNTEST=${RUNTEST};

.include <bsd.regress.mk>

PORTSDIR?=/usr/ports
