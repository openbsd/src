#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$NetBSD: Makefile.inc,v 1.6 1995/10/09 02:25:41 chopps Exp $

.ifdef DESTDIR
snap_md: netbsd-generic
	cp ${.CURDIR}/../sys/arch/amiga/compile/GENERIC/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-generic.gz
netbsd-generic:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/amiga/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 netbsd
.endif	# DESTDIR check
