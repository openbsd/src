#
#	etc.mvme68k/Makefile.inc -- mvme68k-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.3 1995/11/22 10:51:49 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd-generic bootblocks
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd.gz \
	    ${DESTDIR}/snapshot/bsd-generic.gz
bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 bsd
bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/
.endif	# DESTDIR check
