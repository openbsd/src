#	$OpenBSD: Makefile.inc,v 1.13 1998/04/08 00:30:55 niklas Exp $
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets

.ifdef DESTDIR
snap_md: distrib bsd

bsd:
	# The GENERIC kernel is built as a side-effect of the miniroot
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

.endif	# DESTDIR check
