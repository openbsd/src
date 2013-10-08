#	$OpenBSD: Makefile,v 1.6 2013/10/08 21:55:20 miod Exp $

SUBDIR= a2coff installboot vdmtool

.if ${MACHINE} == "aviion"
SUBDIR+=	libprom libsa libz
#SUBDIR+=	netboot
.endif

.include <bsd.subdir.mk>
