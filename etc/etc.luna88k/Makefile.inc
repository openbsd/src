#	$OpenBSD: Makefile.inc,v 1.1 2004/04/21 18:23:44 miod Exp $
#	etc.luna88k/Makefile.inc -- luna88k-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd-generic distrib
	cp ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd

bsd-generic:
	cd ${.CURDIR}/../sys/arch/luna88k/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/luna88k/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}

.PHONY: bsd-generic

.endif	# DESTDIR check
