#	$OpenBSD: Makefile.inc,v 1.1 1999/11/30 21:25:38 mickey Exp $

.ifdef DESTDIR
snap_md: bsd distrib

bsd:
	cd ${.CURDIR}/../sys/arch/hppa/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/hppa/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hppa/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
