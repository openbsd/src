#	$OpenBSD: Makefile.inc,v 1.13 1998/09/17 13:43:07 rahnds Exp $
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
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config MIX
	cd ${.CURDIR}/../sys/arch/powerpc/compile/MIX && \
	    make clean && make depend && make
	(cd  ${.CURDIR}/../sys/arch/powerpc/compile/MIX/ && \
		tar czf ${DESTDIR}/snapshot/bsdmix${OSrev}.tar.gz bsd )

# knows about  bsd.rd, bsdofw.rd and bsdmix.rd
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
	mkdir ${DESTDIR}/snapshot/bootmix
	cp ${DESTDIR}/snapshot/bsdmix.rd ${DESTDIR}/snapshot/bootmix/bsdmix.rd
	strip ${DESTDIR}/snapshot/bootmix/bsdmix.rd
	gzip -9 ${DESTDIR}/snapshot/bootmix/bsdmix.rd
	mv ${DESTDIR}/snapshot/bootmix/bsdmix.rd.gz ${DESTDIR}/snapshot/bootmix/bsd
	cp ${DESTDIR}/usr/mdec/ofwboot ${DESTDIR}/snapshot/bootmix
	mkisofs -r ${DESTDIR}/snapshot/bootmix > ${DESTDIR}/snapshot/bootmix.fs
	rm -rf ${DESTDIR}/snapshot/bootmix
	# compress the bsd.rd files but keep original naming,
	# bootloader can deal with it.
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd
	gzip -9 ${DESTDIR}/snapshot/bsdofw.rd
	mv ${DESTDIR}/snapshot/bsdofw.rd.gz ${DESTDIR}/snapshot/bsdofw.rd
	gzip -9 ${DESTDIR}/snapshot/bsdmix.rd
	mv ${DESTDIR}/snapshot/bsdmix.rd.gz ${DESTDIR}/snapshot/bsdmix.rd


.endif	# DESTDIR check
