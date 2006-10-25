#	$OpenBSD: Makefile.inc,v 1.1 2006/10/25 01:47:41 drahn Exp $

.ifdef DESTDIR
snap_md: bsd bootblocks distrib
	cp ${.CURDIR}/../sys/arch/landisk/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/landisk/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/landisk/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && exec ${MAKE}

# probably will be replaced by procedure to build 'miniroot' image
bootblocks:
	cp ${DESTDIR}/usr/mdec/mbr ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/bootxx ${DESTDIR}/snapshot
	cp ${DESTDIR}/usr/mdec/boot ${DESTDIR}/snapshot

.PHONY: bsd bootblocks

.endif	# DESTDIR check
