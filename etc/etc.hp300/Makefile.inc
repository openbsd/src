#	$OpenBSD: Makefile.inc,v 1.6 1997/10/13 07:45:34 downsj Exp $
#	etc.hp300/Makefile.inc -- hp300-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bsd-dl

bsd:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hp300/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hp300/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd-gen

bsd-dl:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config DISKLESS
	cd ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS/bsd \
	    ${DESTDIR}/snapshot/bsd-dl

.endif	# DESTDIR check
