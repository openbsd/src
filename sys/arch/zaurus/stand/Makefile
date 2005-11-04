#	$OpenBSD: Makefile,v 1.6 2005/11/04 01:02:31 uwe Exp $

SUBDIR= zboot

.if ${MACHINE} == "zaurus"
SUBDIR+= zbsdmod
.endif

.include <bsd.subdir.mk>
