#
#	etc.amiga/Makefile.inc -- amiga-specific etc Makefile targets
#
#	$NetBSD: Makefile.inc,v 1.7 1995/11/28 05:40:21 jtc Exp $

.ifdef DESTDIR
snap_md: netbsd-install
	cp ${.CURDIR}/../sys/arch/amiga/compile/INSTALL/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-install.gz
netbsd-install:
	cd ${.CURDIR}/../sys/arch/amiga/conf && config INSTALL
	cd ${.CURDIR}/../sys/arch/amiga/compile/INSTALL && \
	    make clean && make depend && make && gzip -c -9 netbsd >netbsd.gz
.endif	# DESTDIR check
