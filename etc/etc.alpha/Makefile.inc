#	$OpenBSD: Makefile.inc,v 1.22 2013/03/09 17:25:25 deraadt Exp $

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/bootxx ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/netboot ${RELEASEDIR}

MDEXT=	bsd.rd cd${OSrev}.iso \
	floppy${OSrev}.fs floppyB${OSrev}.fs floppyC${OSrev}.fs \
	boot bootxx netboot
