#
#	etc.sparc/Makefile.inc -- sparc-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.7 1997/09/18 17:17:40 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bsd.scsi3 bootblocks distrib
	cp ${.CURDIR}/../sys/arch/sparc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	cp ${.CURDIR}/../sys/arch/sparc/compile/GENERIC_SCSI3/bsd \
	    ${DESTDIR}/snapshot/bsd.scsi3

bsd:
	cd ${.CURDIR}/../sys/arch/sparc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sparc/compile/GENERIC && \
	    make clean && make depend && make

bsd.scsi3:
	cd ${.CURDIR}/../sys/arch/sparc/conf && config GENERIC_SCSI3
	cd ${.CURDIR}/../sys/arch/sparc/compile/GENERIC_SCSI3 && \
	    make clean && make depend && make

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
