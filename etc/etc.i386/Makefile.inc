#
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.3 1996/06/11 10:20:33 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd
#snap_md: kc.fs		# too big?

bsd:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

kc.fs:
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kc.fs \
	    KERNEL=${DESTDIR}/snapshot/bsd || \
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kc.fs unconfig

.endif	# DESTDIR check
