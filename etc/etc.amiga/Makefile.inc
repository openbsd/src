#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$NetBSD: Makefile.inc,v 1.6.2.2 1995/11/24 19:44:36 chopps Exp $

.ifdef DESTDIR
snap_md: netbsd-install
	cp ${.CURDIR}/../sys/arch/amiga/compile/INSTALL/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-install.gz
netbsd-install:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config INSTALL
	cd ${.CURDIR}/../sys/arch/amiga/compile/INSTALL && \
	    make clean && make depend && make && gzip -c -9 netbsd >netbsd.gz
.endif	# DESTDIR check
