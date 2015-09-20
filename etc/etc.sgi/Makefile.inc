#	$OpenBSD: Makefile.inc,v 1.17 2015/09/20 12:55:11 miod Exp $

ALLIP=	22 26 27 28 30 32
KERNELS=
MDEXT=

.for IP in ${ALLIP}
KERNELS+=	GENERIC-IP${IP} bsd.IP${IP}
MDEXT+=		bsd.rd.IP${IP}
.endfor
KERNELS+=	GENERIC-IP30.MP bsd.mp.IP30

bootblocks:
	cp ${DESTDIR}/usr/mdec/boot32 ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/boot64 ${RELEASEDIR}
	cp ${DESTDIR}/usr/mdec/bootecoff ${RELEASEDIR}

MDEXT+=	boot32 boot64 bootecoff cd${OSrev}.iso
