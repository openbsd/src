#	$OpenBSD: Makefile.inc,v 1.3 1997/09/21 11:50:28 deraadt Exp $
#	etc.mac68k/Makefile.inc -- mac68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bsd-genericsbc
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd-generic
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC/bsd \
	    ${DESTDIR}/snapshot/bsd-genericsbc
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    make clean && make depend && make
bsd-genericsbc:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERICSBC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC && \
	    make clean && make depend && make

.endif	# DESTDIR check
