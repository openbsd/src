#	$OpenBSD: Makefile,v 1.3 2004/08/21 18:54:59 tom Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	etc mbr cdbr
.endif
SUBDIR+= biosboot installboot boot pxeboot cdboot

.include <bsd.subdir.mk>
