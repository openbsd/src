#
#	etc.sun3/Makefile.inc -- sun3-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:38:02 deraadt Exp $

.ifdef DESTDIR
snap_md: netbsd-gen netbsd-dl
	cp -p ${.CURDIR}/etc.sun3/README ${DESTDIR}/snapshot/README

netbsd-gen:
	cd ${.CURDIR}/../sys/arch/sun3/compile/GENERIC && make
	gzip -9 < ${.CURDIR}/../sys/arch/sun3/compile/GENERIC/netbsd \
	  > ${DESTDIR}/snapshot/netbsd-gen.gz

netbsd-dl:
	cd ${.CURDIR}/../sys/arch/sun3/compile/DISKLESS && make
	gzip -9 < ${.CURDIR}/../sys/arch/sun3/compile/DISKLESS/netbsd \
	  > ${DESTDIR}/snapshot/netbsd-dl.gz

.endif	# DESTDIR check
