# $OpenBSD: Makefile,v 1.5 2004/12/29 06:02:14 david Exp $

.include <bsd.own.mk>

NOMAN=
NOPROG=

HAVE_DEJAGNU!=pkg_info dejagnu

.if ${USE_GCC3:L:Myes}
TESTDIR=${BSDSRCDIR}/gnu/usr.bin/gcc
.else
TESTDIR=${BSDSRCDIR}/gnu/egcs/gcc
.endif

REGRESS_TARGETS=do-gcc

.if !defined(DO_DEJATESTS)
REGRESS_SKIP_TARGETS=do-gcc
.endif

.if empty(HAVE_DEJAGNU)
do-gcc: 
	@echo "Error: DejaGnu is required to run this test.";
	@echo "  You can install the package from the ftp site or compile it";
	@echo "  from the ports tree (${PORTSDIR}/devel/dejagnu).";
	@exit 1
.else
do-gcc: 
	@cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	${MAKE} check RUNTEST=${RUNTEST};
.endif

.include <bsd.regress.mk>

PORTSDIR?=/usr/ports
