#	$NetBSD: Makefile.inc,v 1.2 1995/06/26 03:27:15 christos Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree

COMPATDIR=	$S/compat/common

COMPATDST?=      lib/compat
COMPATREL?=      ../../
COMPATLIB=	${COMPATDST}/libcompat.a
COMPATLIB_PROF=	${COMPATDST}/libcompat_p.a

${COMPATLIB}:		.NOTMAIN __always_make_compatlib
	@echo making sure the compat library is up to date...
	@(cd ${COMPATDST} && ${MAKE} -f ${COMPATREL}${COMPATDIR}/Makefile \
	  COMPATCC="${CC}" \
	  COMPATCFLAGS="${CFLAGS}" \
	  COMPATREL="${COMPATREL}" \
	  COMPATDIR="${COMPATDIR}" libcompat.a)

${COMPATLIB_PROF}:	.NOTMAIN __always_make_compatlib
	@echo making sure the profiled compat library is up to date...
	@(cd ${COMPATDST} && ${MAKE} -f ${COMPATREL}${COMPATDIR}/Makefile \
	  COMPATCC="${CC}" \
	  COMPATCFLAGS="${CFLAGS}" \
	  COMPATREL="${COMPATREL}" \
	  COMPATDIR="${COMPATDIR}" libcompat_p.a)

clean:: 		.NOTMAIN __always_make_compatlib
	@echo cleaning the compat library objects
	@(cd ${COMPATDST} && ${MAKE} -f ${COMPATREL}${COMPATDIR}/Makefile \
	  COMPATCC="${CC}" \
	  COMPATCFLAGS="${CFLAGS}" \
	  COMPATREL="${COMPATREL}" \
	  COMPATDIR="${COMPATDIR}" clean)

__always_make_compatlib:	.NOTMAIN
	@([ -d ${COMPATDST} ] || mkdir -p ${COMPATDST})
