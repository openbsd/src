#	$OpenBSD: Makefile.inc,v 1.15 2000/02/03 03:06:55 rahnds Exp $
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib

bsd:
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config GENERICMAC
	cd ${.CURDIR}/../sys/arch/powerpc/compile/GENERICMAC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/GENERICMAC/bsd \
	    ${DESTDIR}/snapshot/bsd

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
	# compress the bsd.rd files but keep original naming,
	# bootloader can deal with it.
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd


.endif	# DESTDIR check
