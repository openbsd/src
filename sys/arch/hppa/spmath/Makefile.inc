#	$OpenBSD: Makefile.inc,v 1.1 1998/06/23 20:33:50 mickey Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

SPMATHDIR=	$S/arch/hppa/spmath

SPMATHDST?=	${.OBJDIR}/lib/spmath
SPMATH_AS?=	obj

SPMATHLIB=	${SPMATHDST}/libspmath.o
SPMATHLIB_PROF=	${SPMATHDST}/libspmath_p.o

SPMATHMAKE= \
	cd ${SPMATHDIR} && MAKEOBJDIR=${SPMATHDST} ${MAKE} \
	    CC='${CC}' CFLAGS='${CFLAGS}' \
	    AS='${AS}' AFLAGS='${AFLAGS}' \
	    LD='${LD}' STRIP='${STRIP}' \
	    CPP='${CPP}' STRIP='${STRIP}' AR='${AR}' \
	    NM='${NM}' LORDER='${LORDER}' \
	    XMACHINE='${MACHINE}' XMACHINE_ARCH='${MACHINE_ARCH}' \
	    SPMATHCPPFLAGS='${CPPFLAGS:S@^-I.@-I../../.@g}'

${SPMATHLIB}:		.NOTMAIN __always_make_kernlib
	@echo making sure the kern library is up to date...
	@${SPMATHMAKE} libkern.o

${SPMATHLIB_PROF}:	.NOTMAIN __always_make_kernlib
	@echo making sure the profiled kern library is up to date...
	@${SPMATHMAKE} libkern.po

clean::			.NOTMAIN __always_make_kernlib
	@echo cleaning the kern library objects
	@${SPMATHMAKE} clean

depend:: 		.NOTMAIN __always_make_kernlib
	@echo depending the kern library objects
	@${SPMATHMAKE} depend

__always_make_kernlib:	.NOTMAIN
	-mkdir -p ${SPMATHDST}
