#
#	etc.mvme68k/Makefile.inc -- mvme68k-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.6 1997/09/18 17:17:37 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd-generic bootblocks distrib
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    make clean && make depend && make

bootblocks:
	cp ${DESTDIR}/usr/mdec/* ${DESTDIR}/snapshot/

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
