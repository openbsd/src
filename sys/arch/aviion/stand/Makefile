#	$OpenBSD: Makefile,v 1.4 2013/09/29 17:51:34 miod Exp $

SUBDIR= a2coff installboot

.if ${MACHINE} == "aviion"
SUBDIR+=	libprom libsa libz
SUBDIR+=	netboot
.endif

.include <bsd.subdir.mk>
