#	$OpenBSD: Makefile.inc,v 1.5 1998/02/16 02:04:39 gene Exp $
#	etc.mac68k/Makefile.inc -- mac68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bsd-genericsbc
	(cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC;
	 tar cf - bsd \
	 | ${GZIP} ${GZIPFLAGS} >${DESTDIR}/snapshot/bsd-generic.tar${GZIPEXT})
	(cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC;
	 tar cf - bsd \
	 | ${GZIP} ${GZIPFLAGS} >${DESTDIR}/snapshot/bsd-genericsbc.tar${GZIPEXT})
bsd-generic:
	(cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC)
	(cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    make clean && make depend && make)
bsd-genericsbc:
	(cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERICSBC)
	(cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC && \
	    make clean && make depend && make)

.endif	# DESTDIR check
