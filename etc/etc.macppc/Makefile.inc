#	$OpenBSD: Makefile.inc,v 1.10 2008/04/25 04:04:15 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bsd.mp bootblocks distrib gzip_bsd.rd
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC.MP/bsd \
	    ${DESTDIR}/snapshot/bsd.mp

bsd:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

bsd.mp:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC.MP
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC.MP && \
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
