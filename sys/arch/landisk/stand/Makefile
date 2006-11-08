#	$OpenBSD: Makefile,v 1.4 2006/11/08 17:46:54 deraadt Exp $

SUBDIR=		mbr xxboot
.if ${MACHINE} == "landisk"
SUBDIR+=	boot
.endif


.include <bsd.subdir.mk>
