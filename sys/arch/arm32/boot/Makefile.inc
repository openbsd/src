#	$NetBSD: Makefile.inc,v 1.1 1996/01/31 23:17:48 mark Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

KERNDIR=	$S/lib/libkern

KERNDST=	lib/kern
KERNREL?=	../../
KERNLIB?=	${KERNDST}/libkern.a

${KERNLIB}:		.NOTMAIN __always_make_kernlib
	@echo making sure the kern library is up to date...
	@(cd ${KERNDST} && ${MAKE} -f ${KERNREL}${KERNDIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  KERNREL="${KERNREL}" \
	  KERNDIR="${KERNDIR}" libkern.o)

clean:: 		.NOTMAIN __always_make_kernlib
	@echo cleaning the kern library objects
	@(cd ${KERNDST} && ${MAKE} -f ${KERNREL}${KERNDIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  KERNREL="${KERNREL}" \
	  KERNDIR="${KERNDIR}" clean)

__always_make_kernlib:	.NOTMAIN
	@([ -d ${KERNDST} ] || mkdir -p ${KERNDST})
