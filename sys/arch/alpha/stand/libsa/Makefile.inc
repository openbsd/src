#	$OpenBSD: Makefile.inc,v 1.2 2011/06/05 21:49:36 miod Exp $
#	$NetBSD: Makefile.inc,v 1.5 1996/09/30 16:01:18 ws Exp $
#
#	NOTE: $S must correspond to the top of the 'sys' tree
SADIR=	$S/lib/libsa

SADST?=	lib/sa
SAREL?=	../../
SALIB?=	${SADST}/libsa.a

SAMAKE= \
	cd ${SADST} && ${MAKE} -f ${SAREL}${SADIR}/Makefile \
	  CC="${CC}" XCFLAGS="${CFLAGS}" \
	  AS='${AS}' XAFLAGS='${AFLAGS}' \
	  LD='${LD}' \
	  SAREL="${SAREL}" \
	  SADIR="${SADIR}" \
	  EXTRACFLAGS="${EXTRACFLAGS}"
.if defined(NO_NET)
SAMAKE+= NO_NET=""
.endif
.if defined(SA_ZLIB)
SAMAKE+= SA_ZLIB=""
.endif

${SALIB}:		.NOTMAIN __always_make_salib
	@echo making sure the stand-alone library is up to date...
	@${SAMAKE} libsa.a

clean:	 		.NOTMAIN __always_make_salib
	@echo cleaning the stand-alone library objects
	@${SAMAKE} clean

depend:			.NOTMAIN __always_make_salib
	@echo depending the standalone library objects
	@${SAMAKE} depend

__always_make_salib:	.NOTMAIN
	@([ -d ${SADST} ] || mkdir -p ${SADST})

