#	$OpenBSD: Makefile.inc,v 1.7 1997/09/21 11:50:25 deraadt Exp $
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd distrib

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
