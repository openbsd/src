#	$OpenBSD: Makefile.inc,v 1.9 1997/10/19 20:22:21 deraadt Exp $
#	etc.hp300/Makefile.inc -- hp300-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bsd-dl bootblocks

bsd:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hp300/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hp300/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-dl:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config DISKLESS
	cd ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS/bsd \
	    ${DESTDIR}/snapshot/bsd-dl

bootblocks:
	-cp -R ${DESTDIR}/usr/mdec/?boot* ??boot boot?? ${DESTDIR}/snapshot

.endif	# DESTDIR check
