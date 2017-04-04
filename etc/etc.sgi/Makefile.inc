#	$OpenBSD: Makefile.inc,v 1.19 2017/04/04 00:03:12 deraadt Exp $

ALLIP=	22 26 27 28 30 32
KERNELS=
MDEXT=

.for IP in ${ALLIP}
KERNELS+=	GENERIC-IP${IP} bsd.IP${IP}
MDEXT+=		bsd.rd.IP${IP}
.endfor
KERNELS+=	GENERIC-IP30.MP bsd.mp.IP30
KERNELS+=	GENERIC-IP27.MP bsd.mp.IP27

bootblocks:
	cp -p ${DESTDIR}/usr/mdec/boot32 ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/boot64 ${RELEASEDIR}
	cp -p ${DESTDIR}/usr/mdec/bootecoff ${RELEASEDIR}

MDEXT+=	boot32 boot64 bootecoff cd${OSrev}.iso
