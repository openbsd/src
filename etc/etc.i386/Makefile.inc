#	$OpenBSD: Makefile.inc,v 1.10 1997/09/26 19:51:28 deraadt Exp $
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd notes distrib

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

notes:
	cp ${.CURDIR}/etc.i386/INSTALL.* ${DESTDIR}/snapshot

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
