#	$OpenBSD: Makefile.inc,v 1.3 1996/05/22 12:06:38 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.14 1996/05/11 16:13:39 mycroft Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

KERNDIR=	$S/lib/libkern

KERNDST=	lib/kern
KERNREL?=	../../
KERNLIB?=	${KERNDST}/libkern.o
KERNLIB_PROF=	${KERNDST}/libkern.po

KERNMAKE= \
	cd ${KERNDST} && ${MAKE} -f ${KERNREL}${KERNDIR}/Makefile \
	    CC="${CC}" CFLAGS="${CFLAGS}" \
	    AS="${AS}" AFLAGS="${AFLAGS}" \
	    LD="${LD}" \
	    MACHINE="${MACHINE}" MACHINE_ARCH="${MACHINE_ARCH}" \
	    KERNCPPFLAGS="${CPPFLAGS}" \
	    KERNREL="${KERNREL}" \
	    KERNDIR="${KERNDIR}"

${KERNLIB}:		.NOTMAIN __always_make_kernlib
	@echo making sure the kern library is up to date...
	@${KERNMAKE} libkern.o

${KERNLIB_PROF}:	.NOTMAIN __always_make_kernlib
	@echo making sure the profiled kern library is up to date...
	@${KERNMAKE} libkern.po

clean:: 		.NOTMAIN __always_make_kernlib
	@echo cleaning the kern library objects
	@${KERNMAKE} clean

depend:: 		.NOTMAIN __always_make_kernlib
	@echo depending the kern library objects
	@${KERNMAKE} depend

__always_make_kernlib:	.NOTMAIN
	@mkdir -p ${KERNDST}
