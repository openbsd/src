#	$OpenBSD: Makefile,v 1.2 2007/11/26 10:01:42 deraadt Exp $

.if ${MACHINE} == "hppa64"
SUBDIR= libsa libz
.endif
SUBDIR+= mkboot boot
.if ${MACHINE} == "hppa64"
#SUBDIR+= cdboot
.endif

.include <bsd.subdir.mk>
