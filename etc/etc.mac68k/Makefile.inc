#	$OpenBSD: Makefile.inc,v 1.8 2002/10/30 21:37:37 miod Exp $
#	etc.mac68k/Makefile.inc -- mac68k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic bsd-genericsbc
	(cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC; \
	 tar cf - bsd \
	 | ${GZIP} ${GZIPFLAGS} >${DESTDIR}/snapshot/bsd.tgz)
	(cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC; \
	 tar cf - bsd \
	 | ${GZIP} ${GZIPFLAGS} >${DESTDIR}/snapshot/bsd-sbc.tgz)
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}
bsd-genericsbc:
	cd ${.CURDIR}/../sys/arch/mac68k/conf && config GENERICSBC
	cd ${.CURDIR}/../sys/arch/mac68k/compile/GENERICSBC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

.PHONY: bsd-generic bsd-genericsbc

.endif	# DESTDIR check
