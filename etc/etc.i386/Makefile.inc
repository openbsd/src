#
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.1.1.1 1995/10/18 08:38:00 deraadt Exp $

.ifdef DESTDIR
snap_md: netbsd-aha netbsd-bt
#snap_md: kc_aha.fs kc_bt.fs

netbsd-aha:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERICAHA
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERICAHA && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERICAHA/netbsd \
	    ${DESTDIR}/snapshot/netbsd-aha

netbsd-bt:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERICBT
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERICBT && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERICBT/netbsd \
	    ${DESTDIR}/snapshot/netbsd-bt

kc_aha.fs:
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kc_aha.fs \
	    KERNEL=${DESTDIR}/snapshot/netbsd-aha || \
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kc_aha.fs unconfig

kc_bt.fs:
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kc_bt.fs \
	    KERNEL=${DESTDIR}/snapshot/netbsd-bt || \
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kc_bt.fs unconfig

.endif	# DESTDIR check
