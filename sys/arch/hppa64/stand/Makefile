#	$OpenBSD: Makefile,v 1.1 2005/04/01 10:40:48 mickey Exp $

.if ${MACHINE} == "hppa64"
SUBDIR= libsa libkern libz
.endif
SUBDIR+= mkboot boot
.if ${MACHINE} == "hppa64"
#SUBDIR+= cdboot
.endif

.include <bsd.subdir.mk>
