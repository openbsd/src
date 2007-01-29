#	$OpenBSD: Makefile.inc,v 1.9 2007/01/29 21:28:33 martin Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib gzip_bsd.rd
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/ofwboot
	cp ${DESTDIR}/usr/mdec/bsd.tbxi ${DESTDIR}/snapshot/bsd.tbxi
	cp ${DESTDIR}/usr/mdec/boot.mac ${DESTDIR}/snapshot/boot.mac

gzip_bsd.rd:
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd

.PHONY: bootblocks bsd gzip_bsd.rd

.endif	# DESTDIR check
