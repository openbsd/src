#
#	etc.sparc/Makefile.inc -- sparc-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.4 1997/05/01 08:56:46 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd bsd.scsi3 floppies miniroot
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

floppies:
	cd ${.CURDIR}/../distrib/sparc/floppies && make cleandir && \
	    make && make install

miniroot:
	cd ${.CURDIR}/../distrib/miniroot && make cleandir && \
	    make && make install

.endif	# DESTDIR check
