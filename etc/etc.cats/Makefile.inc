#	$OpenBSD: Makefile.inc,v 1.2 2004/02/06 02:36:31 drahn Exp $
#	etc.macppc/Makefile.inc -- macppc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib1
	cp ${.CURDIR}/../sys/arch/cats/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/cats/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/cats/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# knows about bsd.rd
distrib1:
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/ofwboot
	${MAKE} distrib
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd

.PHONY: bsd distrib1

.endif	# DESTDIR check
