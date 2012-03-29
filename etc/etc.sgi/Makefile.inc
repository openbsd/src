#	$OpenBSD: Makefile.inc,v 1.15 2012/03/29 20:27:53 miod Exp $

KERNELS = \
	GENERIC-IP22 bsd.IP22 \
	GENERIC-IP27 bsd.IP27 \
	GENERIC-IP30 bsd.IP30 \
	GENERIC-IP30.MP bsd.mp.IP30 \
	GENERIC-IP32 bsd.IP32

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot32 ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/boot64 ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/bootecoff ${RELEASEDIR}

MDEXT=	bsd.rd.IP22 bsd.rd.IP27 bsd.rd.IP30 bsd.rd.IP32 \
	boot32 boot64 bootecoff cd${OSrev}.iso
