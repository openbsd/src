# $OpenBSD: Makefile,v 1.5 2004/12/29 06:02:14 david Exp $

.include <bsd.own.mk>

NOMAN=
NOPROG=

HAVE_DEJAGNU!=pkg_info dejagnu

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

REGRESS_TARGETS=do-libf2c

.if !defined(DO_DEJATESTS)
REGRESS_SKIP_TARGETS=do-libf2c
.endif

.if empty(HAVE_DEJAGNU)
do-libf2c:
	@echo "Error: DejaGnu is required to run this test.";
	@echo "  You can install the package from the ftp site or compile it";
	@echo "  from the ports tree (${PORTSDIR}/devel/dejagnu).";
	@exit 1
.else
do-libf2c: 
	@cd ${TESTDIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	cd ${TEST2DIR} && ${MAKE} -f Makefile.bsd-wrapper && \
	cd ${TEST3DIR} && ${MAKE} G77DIR=${TEST4DIR} check RUNTEST=${RUNTEST};
.endif

.include <bsd.regress.mk>

PORTSDIR?=/usr/ports
