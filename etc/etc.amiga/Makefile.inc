#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$OpenBSD: Makefile.inc,v 1.4 1996/05/09 07:01:05 niklas Exp $
#	$NetBSD: Makefile.inc,v 1.7 1995/11/28 05:40:21 jtc Exp $

.ifdef DESTDIR
snap_md: bsd-install
	cp ${.CURDIR}/../sys/arch/amiga/compile/INSTALL/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-install.gz
bsd-install:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config INSTALL
	cd ${.CURDIR}/../sys/arch/amiga/compile/INSTALL && \
	    make clean && make depend && make && gzip -c -9 bsd >bsd.gz
.endif	# DESTDIR check
