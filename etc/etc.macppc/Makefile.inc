#	$OpenBSD: Makefile.inc,v 1.5 2002/06/09 06:15:14 todd Exp $
#	etc.macppc/Makefile.inc -- macppc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib1
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# knows about bsd.rd
distrib1:
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/ofwboot
	cp ${DESTDIR}/usr/mdec/bsd.tbxi ${DESTDIR}/snapshot/bsd.tbxi
	${MAKE} distrib
	mkdir ${DESTDIR}/snapshot/boot
	cp ${DESTDIR}/snapshot/bsd.rd ${DESTDIR}/snapshot/boot/bsd.rd
	strip ${DESTDIR}/snapshot/boot/bsd.rd
	gzip -9 ${DESTDIR}/snapshot/boot/bsd.rd
	mv ${DESTDIR}/snapshot/boot/bsd.rd.gz ${DESTDIR}/snapshot/boot/bsd
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/boot
	mkisofs -r ${DESTDIR}/snapshot/boot > ${DESTDIR}/snapshot/boot.fs
	rm -rf ${DESTDIR}/snapshot/boot
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd

.PHONY: bsd distrib1

.endif	# DESTDIR check
