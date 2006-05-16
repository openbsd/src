#	$OpenBSD: Makefile,v 1.2 2006/05/16 22:48:16 miod Exp $

SUBDIR= a2coff

.if ${MACHINE} == "aviion"
SUBDIR+= libprom libsa libz netboot
.endif

.include <bsd.subdir.mk>
