#	$OpenBSD: Makefile.inc,v 1.21 2011/07/22 09:30:54 espie Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/*boot* ${RELEASEDIR}
	${DESTDIR}/usr/mdec/binstall net /tmp && \
	    mv /tmp/boot.sparc.openbsd ${RELEASEDIR}/boot.net

MDEXT=	bsd.rd cd${OSrev}.iso \
	floppy${OSrev}.fs miniroot${OSrev}.fs \
	boot boot.net bootxx installboot
