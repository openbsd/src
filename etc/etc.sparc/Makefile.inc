#
#	etc.sparc/Makefile.inc -- sparc-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.2 1995/10/27 23:04:51 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bsd.scsi3 inst
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

inst:
	cp ${.CURDIR}/etc.sparc/README ${DESTDIR}/snapshot/README
	cp ${.CURDIR}/etc.sparc/install.sh ${DESTDIR}/snapshot/install.sh

.endif	# DESTDIR check
