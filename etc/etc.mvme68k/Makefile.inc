#
#	etc.mvme68k/Makefile.inc -- mvme68k-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.1 1995/10/24 14:43:22 deraadt Exp $

.ifdef DESTDIR
snap_md: netbsd-generic
	cp ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-generic.gz
netbsd-generic:
	cd ${.CURDIR}/../sys/arch/mvme68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/mvme68k/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 netbsd
.endif	# DESTDIR check
