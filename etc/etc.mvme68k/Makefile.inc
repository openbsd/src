#
#	etc.mvme68k/Makefile.inc -- mvme68k-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.5 1997/05/10 22:45:16 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd-generic bootblocks ramdisk
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    make clean && make depend && make

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

ramdisk:
	cd ${.CURDIR}/../distrib/mvme68k/ramdisk && make cleandir && \
	    make && make install

.endif	# DESTDIR check
