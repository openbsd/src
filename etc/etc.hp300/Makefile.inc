#	$NetBSD: Makefile.inc,v 1.1 1995/09/06 16:17:41 thorpej Exp $
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
	gzip -9 ${DESTDIR}/snapshot/netbsd-gen

netbsd-dl.gz: netbsd-dl
	gzip -9 ${DESTDIR}/snapshot/netbsd-dl

.endif	# DESTDIR check
