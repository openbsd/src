# $OpenBSD: Makefile,v 1.4 2004/12/29 06:02:14 david Exp $

NOMAN=
NOPROG=

HAVE_DEJAGNU!=pkg_info dejagnu

TESTDIR=${BSDSRCDIR}/gnu/lib/libiberty

REGRESS_TARGETS=do-libiberty

.if !defined(DO_DEJATESTS)
REGRESS_SKIP_TARGETS=do-libiberty
.endif

.if empty(HAVE_DEJAGNU)
do-libiberty:
	@echo "Error: DejaGnu is required to run this test.";
	@echo "  You can install the package from the ftp site or compile it";
	@echo "  from the ports tree (${PORTSDIR}/devel/dejagnu).";
	@exit 1
.else
do-libiberty: 
	@cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	${MAKE} check RUNTEST=${RUNTEST};
.endif

.include <bsd.regress.mk>

PORTSDIR?=/usr/ports
