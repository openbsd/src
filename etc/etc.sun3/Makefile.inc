#
#	etc.sun3/Makefile.inc -- sun3-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.2 1997/05/03 15:25:02 kstailey Exp $

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
