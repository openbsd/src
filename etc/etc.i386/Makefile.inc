#
#	etc.i386/Makefile.inc -- i386-specific etc Makefile targets
#
#	$Id: Makefile.inc,v 1.2 1995/11/18 15:44:24 deraadt Exp $

.ifdef DESTDIR
snap_md: bsd.adp bsd.other
#snap_md: kcadp.fs kcother.fs

bsd.adp:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERICADP
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERICADP && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERICADP/bsd \
	    ${DESTDIR}/snapshot/bsd.adp

bsd.other:
	cd ${.CURDIR}/../sys/arch/i386/conf && config GENERICOTHER
	cd ${.CURDIR}/../sys/arch/i386/compile/GENERICOTHER && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/i386/compile/GENERICOTHER/bsd \
	    ${DESTDIR}/snapshot/bsd.other

kcadp.fs:
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kcadp.fs \
	    KERNEL=${DESTDIR}/snapshot/bsd.adp || \
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kcadp.fs unconfig

kcother.fs:
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kcother.fs \
	    KERNEL=${DESTDIR}/snapshot/bsd.other || \
	make -f ${.CURDIR}/etc.i386/Makefile.kc \
	    IMAGE=${DESTDIR}/snapshot/kcother.fs unconfig

.endif	# DESTDIR check
