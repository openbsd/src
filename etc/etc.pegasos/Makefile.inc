#	$OpenBSD: Makefile.inc,v 1.1 2003/10/31 03:56:54 drahn Exp $
#	etc.macppc/Makefile.inc -- macppc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib1
	cp ${.CURDIR}/../sys/arch/pegasos/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/pegasos/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/pegasos/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# knows about bsd.rd
distrib1:
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/ofwboot
	${MAKE} distrib
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd

.PHONY: bsd distrib1

.endif	# DESTDIR check
