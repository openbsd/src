#	$OpenBSD: Makefile.inc,v 1.4 1997/10/06 20:10:52 gene Exp $
#	etc.mac68k/Makefile.inc -- mac68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bsd-genericsbc
	tar cf - ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC/bsd \
	| ${GZIP} ${GZIPFLAGS} >${DESTDIR}/snapshot/bsd-generic.tar${GZIPEXT}
	tar cf - ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC/bsd \
	| ${GZIP} ${GZIPFLAGS} >${DESTDIR}/snapshot/bsd-genericsbc.tar${GZIPEXT}
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    make clean && make depend && make
bsd-genericsbc:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERICSBC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC && \
	    make clean && make depend && make

.endif	# DESTDIR check
