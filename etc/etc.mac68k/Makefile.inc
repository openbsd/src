#	$OpenBSD: Makefile.inc,v 1.9 2004/12/03 06:37:51 miod Exp $
#	etc.mac68k/Makefile.inc -- mac68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	printf "disable ncrscsi\nenable sbc\nquit" | config -e \
	    -o ${DESTDIR}/snapshot/bsdsbc ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

.PHONY: bsd

.endif	# DESTDIR check
