#
#	etc.mvme68k/Makefile.inc -- mvme68k-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.4 1997/05/01 08:56:34 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd-generic bootblocks ramdisk
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 bsd
bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

ramdisk:
	cd ${.CURDIR}/../distrib/mvme68k/ramdisk && make cleandir && \
	    make && make install

.endif	# DESTDIR check
