#
#	$OpenBSD: Makefile.inc,v 1.1 1997/05/11 02:03:55 gene Exp $
#	etc.mac68k/Makefile.inc -- mac68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bsd-genericsbc
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz
	cp ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-genericsbc.gz
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 bsd
bsd-genericsbc:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERICSBC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC && \
	    make clean && make depend && make && gzip -9 bsd

.endif	# DESTDIR check
