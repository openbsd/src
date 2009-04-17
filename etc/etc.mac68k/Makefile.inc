#	$OpenBSD: Makefile.inc,v 1.11 2009/04/17 03:58:54 deraadt Exp $

kernels: bsd
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	printf "disable ncrscsi\nenable sbc\nquit" | config -e \
	    -o ${RELEASEDIR}/bsdsbc ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

.PHONY: bsd
