#	$NetBSD: Makefile.inc,v 1.4 1995/10/01 06:00:38 phil Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree
SADIR=	$S/lib/libsa

SADST=	lib/sa
SAREL?=	../../
SALIB?=	${SADST}/libsa.a

${SALIB}:		.NOTMAIN __always_make_salib
	@echo making sure the stand-alone library is up to date...
.if !defined(NO_NET)
	@(cd ${SADST} && ${MAKE} -f ${SAREL}${SADIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  SAREL="${SAREL}" \
	  SADIR="${SADIR}" libsa.a)
.else
	@(cd ${SADST} && ${MAKE} -f ${SAREL}${SADIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  SAREL="${SAREL}" \
	  SADIR="${SADIR}" \
	  NO_NET="" libsa.a)
.endif

clean:: 		.NOTMAIN __always_make_salib
	@echo cleaning the stand-alone library objects
	@(cd ${SADST} && ${MAKE} -f ${SAREL}${SADIR}/Makefile \
	  KERNCC="${CC}" \
	  KERNCFLAGS="${CFLAGS}" \
	  SAREL="${SAREL}" \
	  SADIR="${SADIR}" clean)

__always_make_salib:	.NOTMAIN
	@([ -d ${SADST} ] || mkdir -p ${SADST})

