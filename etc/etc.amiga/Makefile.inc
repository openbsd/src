#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$OpenBSD: Makefile.inc,v 1.7 1997/05/13 16:23:06 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.7 1995/11/28 05:40:21 jtc Exp $

.ifdef DESTDIR
snap_md: bsd-install miniroot-install

bsd-install:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amiga/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE} && \
	    cp bsd ${DESTDIR}/snapshot/bsd

miniroot-install:
	cd ${.CURDIR}/../distrib/miniroot && \
	    ${MAKE} cleandir && ${MAKE} && ${MAKE} install

.endif	# DESTDIR check
