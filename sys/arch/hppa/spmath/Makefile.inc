#	$OpenBSD: Makefile.inc,v 1.5 2002/02/04 22:17:56 mickey Exp $
#
#	NOTE:	$S must correspond to the top of the 'sys' tree
#		$HPPA must correspond to the top of hppa tree

SPMATHDIR=	${HPPA}/spmath

SPMATHDST?=	${.OBJDIR}/lib/spmath

SPMATH=		${SPMATHDST}/spmath.o
SPMATH_PROF=	${SPMATHDST}/spmath.po

SPMATHMAKE= \
	cd ${SPMATHDIR} && MAKEOBJDIR=${SPMATHDST} ${MAKE} \
	    CC='${CC}' CFLAGS='${CFLAGS}' \
	    CPPFLAGS='${CPPFLAGS:S@^-I.@-I../../.@g}'\
	    AS='${AS}' AFLAGS='${AFLAGS}' \
	    LD='${LD}' STRIP='${STRIP}' \
	    CPP='${CPP}' STRIP='${STRIP}' AR='${AR}' \
	    NM='${NM}' LORDER='${LORDER}' \
	    XMACHINE='${MACHINE}' XMACHINE_ARCH='${MACHINE_ARCH}' \

${SPMATH}:	.NOTMAIN __always_make_spmath
	@echo making sure the spmath library is up to date...
	@${SPMATHMAKE} spmath.o

${SPMATH_PROF}:	.NOTMAIN __always_make_spmath
	@echo making sure the profiled spmath library is up to date...
	@${SPMATHMAKE} spmath.po

clean::		.NOTMAIN __always_make_spmath
	@echo cleaning the spmath library objects
	@${SPMATHMAKE} clean

depend::	.NOTMAIN __always_make_spmath
	@echo depending the spmath library objects
	@${SPMATHMAKE} depend

__always_make_spmath:	.NOTMAIN
	-mkdir -p ${SPMATHDST}

