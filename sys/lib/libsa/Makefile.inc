#	$NetBSD: Makefile.inc,v 1.4 1995/10/01 06:00:38 phil Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree
SADIR=	$S/lib/libsa

SADST=	lib/sa
SAREL?=	../../
SALIB?=	${SADST}/libsa.a

SAMAKE= \
	cd ${SADST} && ${MAKE} -f ${SAREL}${SADIR}/Makefile \
	  CC="${CC}" XCFLAGS="${CFLAGS}" \
	  AS='${AS}' XAFLAGS='${AFLAGS}' \
	  LD='${LD}' \
	  XCPPFLAGS="${CPPFLAGS}" \
	  SAREL="${SAREL}" \
	  SADIR="${SADIR}"

${SALIB}:		.NOTMAIN __always_make_salib
	@echo making sure the stand-alone library is up to date...
.if !defined(NO_NET)
	@${SAMAKE} libsa.a
.else
	@${SAMAKE} NO_NET="" libsa.a
.endif

clean:: 		.NOTMAIN __always_make_salib
	@echo cleaning the stand-alone library objects
	@${SAMAKE} clean

depend::		.NOTMAIN __always_make_salib
	@echo depending the standalone library objects
	@${SAMAKE} depend

__always_make_salib:	.NOTMAIN
	@([ -d ${SADST} ] || mkdir -p ${SADST})

