#	$OpenBSD: Makefile.inc,v 1.3 2004/02/06 02:39:50 drahn Exp $
#	etc.cats/Makefile.inc -- cats-specific etc Makefile targets

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
	${MAKE} distrib
	mv ${DESTDIR}/snapshot/bsd.rd ${DESTDIR}/snapshot/bsd.rd

.PHONY: bsd distrib1

.endif	# DESTDIR check
