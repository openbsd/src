#
#	$NetBSD: Makefile.inc,v 1.1.1.1 1995/03/30 05:44:08 leo Exp $
#
#	etc.atari/Makefile.inc -- atari-specific etc Makefile targets
#

.ifdef DESTDIR
snap_md: netbsd-generic netbsd-ataritt
	cp ${.CURDIR}/../sys/arch/atari/compile/GENERIC/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-generic.gz
	cp ${.CURDIR}/../sys/arch/atari/compile/ATARITT/netbsd.gz \
	    ${DESTDIR}/snapshot/netbsd-ataritt.gz
netbsd-generic:
	cd ${.CURDIR}/../sys/arch/atari/conf && config.new GENERIC
	cd ${.CURDIR}/../sys/arch/atari/compile/GENERIC && \
	    make clean && make depend && make && gzip -9 netbsd

netbsd-ataritt:
	cd ${.CURDIR}/../sys/arch/atari/conf && config.new ATARITT
	cd ${.CURDIR}/../sys/arch/atari/compile/ATARITT && \
	    make clean && make depend && make && gzip -9 netbsd
.endif	# DESTDIR check
