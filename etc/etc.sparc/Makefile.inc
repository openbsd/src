#	$OpenBSD: Makefile.inc,v 1.10 2002/04/26 18:57:11 deraadt Exp $
#	etc.sparc/Makefile.inc -- sparc-specific etc Makefile targets

.ifdef DESTDIR
snap_md: bsd bootblocks distrib

bsd:
	cd ${.CURDIR}/../sys/arch/sparc/conf && config GENERIC
	cd ${.CURDIR}/../sys/arch/sparc/compile/GENERIC && \
	    make clean && make depend && make
	cp ${.CURDIR}/../sys/arch/sparc/compile/GENERIC/bsd \
	    ${DESTDIR}/snapshot/bsd
	s=`printf "find sd\nexit" | config -e ${DESTDIR}/snapshot/bsd | \
	    grep "ukc>  " | awk '{print $$2}'`; \
	    printf "add sd0\n%s\n%s\nchange %s\ny\n3\n\n\nquit\n" $$s $$s $$s | \
	    config -e -o ${DESTDIR}/snapshot/bsd.scsi3 ${DESTDIR}/snapshot/bsd

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${DESTDIR}/snapshot
	${DESTDIR}/usr/mdec/binstall net /tmp && \
	  mv /tmp/boot.sparc.openbsd ${DESTDIR}/snapshot/boot.net

distrib:
	cd ${.CURDIR}/../distrib && make cleandir && \
	    make && make install

.endif	# DESTDIR check
