#	$OpenBSD: Makefile.inc,v 1.3 1997/09/21 11:50:47 deraadt Exp $
#
#	etc.x68k/Makefile.inc -- x68k-specific etc Makefile targets
#

.ifdef DESTDIR
LOCALTIME=	Japan

snap_md: netbsd-generic netbsd-all
	cp ${.CURDIR}/../sys/arch/x68k/compile/GENERIC/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-generic.gz
	cp ${.CURDIR}/../sys/arch/x68k/compile/ALL/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-all.gz

netbsd-generic:
	cd ${.CURDIR}/../sys/arch/x68k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/x68k/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 netbsd

netbsd-all:
	cd ${.CURDIR}/../sys/arch/x68k/conf && config ALL
	cd ${.CURDIR}/../sys/arch/x68k/compile/ALL && \
	    make clean && make depend && make && gzip -9 netbsd

.endif	# DESTDIR check
