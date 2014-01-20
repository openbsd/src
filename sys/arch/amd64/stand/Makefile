#	$OpenBSD: Makefile,v 1.6 2014/01/20 08:43:11 jsing Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	mbr cdbr
.endif
SUBDIR+=biosboot boot cdboot fdboot pxeboot

.include <bsd.subdir.mk>
