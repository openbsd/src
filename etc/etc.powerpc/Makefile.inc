#	$OpenBSD: Makefile.inc,v 1.6 1998/05/29 04:22:19 rahnds Exp $
#	etc.powerpc/Makefile.inc -- powerpc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: binutils bsd distrib

binutils:
	cd ${DESTDIR} && tar cf - usr/*openbsd* \
	    | ${GZIP} ${GZIPFLAGS} > snapshot/usr.binutils.tar${GZIPEXT}

bsd:
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cd ${.CURDIR}/../sys/arch/powerpc/conf && config DDBOFW
	cd ${.CURDIR}/../sys/arch/powerpc/compile/DDBOFW && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/powerpc/compile/DDBOFW/bsd \
	    ${DESTDIR}/snapshot/bsdofw

# knows about bsdofw.rd and bsd.rd
distrib:
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


.endif	# DESTDIR check
