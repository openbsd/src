#
#	etc.arm32/Makefile.inc -- arm32-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.1 1996/05/01 13:46:59 deraadt Exp $

.ifdef DESTDIR
snap_md: netbsd-gen

netbsd-gen:
	cd ${.CURDIR}/../sys/arch/arm32/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/arm32/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/arm32/compile/GENERIC/netbsd \
	    ${DESTDIR}/snapshot/netbsd-gen

.endif	# DESTDIR check
