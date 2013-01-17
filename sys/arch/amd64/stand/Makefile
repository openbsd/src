#	$OpenBSD: Makefile,v 1.5 2013/01/17 12:37:44 jsing Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	mbr cdbr
.endif
SUBDIR+=biosboot boot cdboot fdboot installboot pxeboot

.include <bsd.subdir.mk>
