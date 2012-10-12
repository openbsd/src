#	$OpenBSD: Makefile,v 1.4 2012/10/12 14:00:02 jsing Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	etc mbr cdbr
.endif
SUBDIR+= biosboot boot cdboot fdboot installboot pxeboot

.include <bsd.subdir.mk>
