#	$OpenBSD: Makefile.inc,v 1.5 1996/05/23 08:32:21 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/05/20 17:23:59 mrg Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

COMPATDIR=	$S/compat/common

COMPATDST?=	lib/compat
COMPATREL?=	../../
COMPATLIB=	${COMPATDST}/libcompat.a
COMPATLIB_PROF=	${COMPATDST}/libcompat_p.a

COMPATMAKE= \
	cd ${COMPATDST} && ${MAKE} -f ${COMPATREL}${COMPATDIR}/Makefile \
	    CC="${CC}" CFLAGS="${CFLAGS:S/-I./-I${COMPATREL}./g}" \
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

depend:: 		.NOTMAIN __always_make_compatlib
	@echo depending the compat library objects
	@${COMPATMAKE} depend

__always_make_compatlib:	.NOTMAIN
	@mkdir -p ${COMPATDST}
