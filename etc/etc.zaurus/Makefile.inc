#	$OpenBSD: Makefile.inc,v 1.2 2004/12/31 00:19:56 miod Exp $
#	etc.zaurus/Makefile.inc -- zaurus-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib1
	cp ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/zaurus/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/zaurus/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# knows about bsd.rd
distrib1:
	${MAKE} distrib

.PHONY: bsd distrib1

.endif	# DESTDIR check
