#	$OpenBSD: Makefile.inc,v 1.3 1997/09/21 11:50:40 deraadt Exp $
#
#	etc.sun3/Makefile.inc -- sun3-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-gen bsd-dl
	cp -p ${.CURDIR}/etc.sun3/README ${DESTDIR}/snapshot/README

bsd-gen:
	cd ${.CURDIR}/../sys/arch/sun3/compile/GENERIC && make
	gzip -9 < ${.CURDIR}/../sys/arch/sun3/compile/GENERIC/bsd \
	  > ${DESTDIR}/snapshot/bsd-gen.gz

bsd-dl:
	cd ${.CURDIR}/../sys/arch/sun3/compile/DISKLESS && make
	gzip -9 < ${.CURDIR}/../sys/arch/sun3/compile/DISKLESS/bsd \
	  > ${DESTDIR}/snapshot/bsd-dl.gz

.endif	# DESTDIR check
