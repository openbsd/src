#	$OpenBSD: Makefile,v 1.3 2003/03/28 22:42:26 mickey Exp $

.if ${MACHINE} == "hppa"
SUBDIR= libsa libkern libz
.endif
SUBDIR+= mkboot boot
.if ${MACHINE} == "hppa"
SUBDIR+= cdboot
.endif

.include <bsd.subdir.mk>
