#	$OpenBSD: Makefile.inc,v 1.2 1996/02/26 23:26:49 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.3 1996/02/02 20:34:44 mycroft Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

COMPATDIR=	$S/compat/common

COMPATDST?=	lib/compat
COMPATREL?=	../../
COMPATLIB=	${COMPATDST}/libcompat.a
COMPATLIB_PROF=	${COMPATDST}/libcompat_p.a

COMPATMAKE= \
	cd ${COMPATDST} && ${MAKE} -f ${COMPATREL}${COMPATDIR}/Makefile \
	    CC="${CC}" CFLAGS="${CFLAGS}" \
	    LD="${LD}" \
	    COMPATCPPFLAGS="${CPPFLAGS}" \
	    COMPATREL="${COMPATREL}" \
	    COMPATDIR="${COMPATDIR}"

${COMPATLIB}:		.NOTMAIN __always_make_compatlib
	@echo making sure the compat library is up to date...
	@${COMPATMAKE} libcompat.a

${COMPATLIB_PROF}:	.NOTMAIN __always_make_compatlib
	@echo making sure the profiled compat library is up to date...
	@${COMPATMAKE} libcompat_p.a

clean:: 		.NOTMAIN __always_make_compatlib
	@echo cleaning the compat library objects
	@${COMPATMAKE} clean

__always_make_compatlib:	.NOTMAIN
	@mkdir -p ${COMPATDST}
