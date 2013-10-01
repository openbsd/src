#	$OpenBSD: Makefile,v 1.5 2013/10/01 20:32:29 miod Exp $

SUBDIR= a2coff installboot vdmtool

.if ${MACHINE} == "aviion"
SUBDIR+=	libprom libsa libz
SUBDIR+=	netboot
.endif

.include <bsd.subdir.mk>
