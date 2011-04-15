#	$OpenBSD: Makefile.inc,v 1.19 2011/04/15 03:11:38 deraadt Exp $

kernels: bsd bootblocks
	cp ${.CURDIR}/../sys/arch/sparc/compile/GENERIC/bsd \
	    ${RELEASEDIR}/bsd
	s=`printf "find sd\nexit" | config -e ${RELEASEDIR}/bsd | \
	    grep scsibus | awk '{print $$1}'`; \
	    printf "add sd0\n%s\n%s\nchange %s\ny\n3\n\n\nquit\n" $$s $$s $$s | \
	    config -e -o ${RELEASEDIR}/bsd.scsi3 ${RELEASEDIR}/bsd

bsd:
	cd ${.CURDIR}/../sys/arch/sparc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sparc/compile/GENERIC && \
	    ${MAKE} clean && exec ${MAKE}

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${RELEASEDIR}
	${DESTDIR}/usr/mdec/binstall net /tmp && \
	    mv /tmp/boot.sparc.openbsd ${RELEASEDIR}/boot.net

MDEXT=	bsd bsd.rd bsd.scsi3 cd${OSrev}.iso \
	floppy${OSrev}.fs miniroot${OSrev}.fs \
	boot boot.net bootxx installboot

.PHONY: bsd bootblocks
