#	$OpenBSD: Makefile.inc,v 1.5 1997/09/21 11:50:23 deraadt Exp $
#	etc.hp300/Makefile.inc -- hp300-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-gen.gz bsd-dl.gz

bsd-gen:
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

bsd-gen.gz: bsd-gen
	gzip -f9 ${DESTDIR}/snapshot/bsd-gen

bsd-dl.gz: bsd-dl
	gzip -f9 ${DESTDIR}/snapshot/bsd-dl

.endif	# DESTDIR check
