#	$OpenBSD: Makefile.inc,v 1.1 2004/12/31 00:06:15 drahn Exp $
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

.PHONY: bsd distrib1

.endif	# DESTDIR check
