#	$NetBSD: Makefile.inc,v 1.12 1995/10/07 09:56:55 mycroft Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

KERNDIR=	$S/lib/libkern

KERNDST=	lib/kern
KERNREL?=	../../
KERNLIB?=	${KERNDST}/libkern.o
KERNLIB_PROF=	${KERNDST}/libkern.po

${KERNLIB}:		.NOTMAIN __always_make_kernlib
	@echo making sure the kern library is up to date...
	@(cd ${KERNDST} && ${MAKE} -f ${KERNREL}${KERNDIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  KERNREL="${KERNREL}" \
	  KERNDIR="${KERNDIR}" libkern.o)

${KERNLIB_PROF}:	.NOTMAIN __always_make_kernlib
	@echo making sure the profiled kern library is up to date...
	@(cd ${KERNDST} && ${MAKE} -f ${KERNREL}${KERNDIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  KERNREL="${KERNREL}" \
	  KERNDIR="${KERNDIR}" libkern.po)

clean:: 		.NOTMAIN __always_make_kernlib
	@echo cleaning the kern library objects
	@(cd ${KERNDST} && ${MAKE} -f ${KERNREL}${KERNDIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  KERNREL="${KERNREL}" \
	  KERNDIR="${KERNDIR}" clean)

__always_make_kernlib:	.NOTMAIN
	@([ -d ${KERNDST} ] || mkdir -p ${KERNDST})
