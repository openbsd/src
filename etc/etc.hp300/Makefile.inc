#	$OpenBSD: Makefile.inc,v 1.3 1997/02/03 07:02:10 downsj Exp $
#	$NetBSD: Makefile.inc,v 1.2 1996/03/22 05:14:27 scottr Exp $
#
#	etc.hp300/Makefile.inc -- hp300-specific etc Makefile targets

.ifdef DESTDIR
snap_md: netbsd-gen.gz netbsd-dl.gz

netbsd-gen:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config.old GENERIC
	cd ${.CURDIR}/../sys/arch/hp300/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hp300/compile/GENERIC/netbsd \
	    ${DESTDIR}/snapshot/netbsd-gen

netbsd-dl:
	cd ${.CURDIR}/../sys/arch/hp300/conf && config.old DISKLESS
	cd ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/hp300/compile/DISKLESS/netbsd \
	    ${DESTDIR}/snapshot/netbsd-dl

netbsd-gen.gz: netbsd-gen
	gzip -f9 ${DESTDIR}/snapshot/netbsd-gen

netbsd-dl.gz: netbsd-dl
	gzip -f9 ${DESTDIR}/snapshot/netbsd-dl

.endif	# DESTDIR check
