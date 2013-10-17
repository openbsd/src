#	$OpenBSD: Makefile,v 1.8 2013/10/17 16:30:07 miod Exp $

SUBDIR= a2coff installboot vdmtool

.if ${MACHINE} == "aviion"
SUBDIR+=	libprom libsa libz
SUBDIR+=	boot
.endif

.include <bsd.subdir.mk>
