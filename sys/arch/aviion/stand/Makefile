#	$OpenBSD: Makefile,v 1.3 2013/09/28 21:59:38 miod Exp $

SUBDIR= a2coff

.if ${MACHINE} == "aviion"
SUBDIR+=	libprom libsa libz
SUBDIR+=	netboot
.endif

.include <bsd.subdir.mk>
