#	$OpenBSD: Makefile.inc,v 1.7 2002/07/14 09:20:24 mdw Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/05/20 17:23:59 mrg Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

PWD!=		pwd
COMPATDIR!=	cd $S/compat/common; pwd
COMPATDST?=	lib/compat
COMPATLIB=	${COMPATDST}/libcompat.a
COMPATLIB_PROF=	${COMPATDST}/libcompat_p.a

COMPATMAKE= \
	cd ${COMPATDST} && ${MAKE} -f ${COMPATDIR}/Makefile \
	    CC="${CC}" CFLAGS="${CFLAGS:S/-I./-I${PWD}\/./g}" \
	    LD="${LD}" AR="${AR}" RANLIB="${RANLIB}" LORDER="${LORDER}" \
	    NM="${NM}" \
	    XMACHINE='${MACHINE}' XMACHINE_ARCH='${MACHINE_ARCH}' \
	    CPPFLAGS="${CPPFLAGS:S/-I./-I${PWD}\/./g}" \
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
