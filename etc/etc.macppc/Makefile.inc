#	$OpenBSD: Makefile.inc,v 1.3 2002/04/22 01:50:51 deraadt Exp $
#	etc.macppc/Makefile.inc -- macppc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib

bsd:
	cd ${.CURDIR}/../sys/arch/macppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/macppc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/macppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

# knows about bsd.rd 
distrib:
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/ofwboot
	cp ${DESTDIR}/usr/mdec/bsd.tbxi ${DESTDIR}/snapshot/bsd.tbxi
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install
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

.endif	# DESTDIR check
