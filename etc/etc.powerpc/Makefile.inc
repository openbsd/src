#	$OpenBSD: Makefile.inc,v 1.14 1998/10/20 23:22:17 rahnds Exp $
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib

bsd:
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config DDBOFW
	cd ${.CURDIR}/../sys/arch/powerpc/compile/DDBOFW && \
	    make clean && make depend && make
	(cd  ${.CURDIR}/../sys/arch/powerpc/compile/DDBOFW/ && \
		tar czf ${DESTDIR}/snapshot/bsdofw${OSrev}.tar.gz bsd )

# knows about  bsd.rd and bsdofw.rd
distrib:
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/ofwboot
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
	#
	mkdir ${DESTDIR}/snapshot/bootofw
	cp ${DESTDIR}/snapshot/bsdofw.rd ${DESTDIR}/snapshot/bootofw/bsdofw.rd
	strip ${DESTDIR}/snapshot/bootofw/bsdofw.rd
	gzip -9 ${DESTDIR}/snapshot/bootofw/bsdofw.rd
	mv ${DESTDIR}/snapshot/bootofw/bsdofw.rd.gz ${DESTDIR}/snapshot/bootofw/bsd
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/bootofw
	cp ${DESTDIR}/usr/mdec/boot.mac ${DESTDIR}/snapshot/bootofw
	mkisofs -r ${DESTDIR}/snapshot/bootofw > ${DESTDIR}/snapshot/bootofw.fs
	rm -rf ${DESTDIR}/snapshot/bootofw
	#
	# compress the bsd.rd files but keep original naming,
	# bootloader can deal with it.
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd
	gzip -9 ${DESTDIR}/snapshot/bsdofw.rd
	mv ${DESTDIR}/snapshot/bsdofw.rd.gz ${DESTDIR}/snapshot/bsdofw.rd


.endif	# DESTDIR check
