#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$OpenBSD: Makefile.inc,v 1.8 1997/05/18 15:28:13 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.7 1995/11/28 05:40:21 jtc Exp $

.ifdef DESTDIR
snap_md: bsd-install miniroot-install ramdisk-install

bsd-install:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amiga/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE} && \
	    cp bsd ${DESTDIR}/snapshot/bsd

miniroot-install:
	cd ${.CURDIR}/../distrib/miniroot && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

ramdisk-install:
	cd ${.CURDIR}/../distrib/ramdisk && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

.endif	# DESTDIR check
