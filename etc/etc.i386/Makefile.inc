#	$OpenBSD: Makefile.inc,v 1.8 1997/09/25 02:00:51 weingart Exp $
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
	cp ${.CURDIR}/INSTALL.* ${DESTDIR}

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
