#	$OpenBSD: Makefile.inc,v 1.2 2002/05/13 07:22:43 espie Exp $
#	etc.mvmeppc/Makefile.inc -- mvmeppc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib1
	cp ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/mvmeppc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvmeppc/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# knows about  bsd.rd and bsdofw.rd
distrib1:
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot/bootxx
	cp ${DESTDIR}/usr/mdec/bootsd ${DESTDIR}/snapshot/bootsd
	cp ${DESTDIR}/usr/mdec/installboot ${DESTDIR}/snapshot/installboot
	${MAKE} distrib
	mkdir ${DESTDIR}/snapshot/boot
	cp ${DESTDIR}/snapshot/bsd.rd ${DESTDIR}/snapshot/boot/bsd.rd
	strip ${DESTDIR}/snapshot/boot/bsd.rd
	gzip -9 ${DESTDIR}/snapshot/boot/bsd.rd
	mv ${DESTDIR}/snapshot/boot/bsd.rd.gz ${DESTDIR}/snapshot/boot/bsd
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot/boot
	mkisofs -r ${DESTDIR}/snapshot/boot > ${DESTDIR}/snapshot/boot.fs
	rm -rf ${DESTDIR}/snapshot/boot
	#
	# compress the bsd.rd files but keep original naming,
	# bootloader can deal with it.
	gzip -9 ${DESTDIR}/snapshot/bsd.rd
	mv ${DESTDIR}/snapshot/bsd.rd.gz ${DESTDIR}/snapshot/bsd.rd


.PHONY: bsd distrib1

.endif	# DESTDIR check
