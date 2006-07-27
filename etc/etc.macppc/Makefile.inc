#	$OpenBSD: Makefile.inc,v 1.8 2006/07/27 02:53:55 deraadt Exp $

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

gzip_bsd.rd:
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd

.PHONY: bootblocks bsd gzip_bsd.rd

.endif	# DESTDIR check
