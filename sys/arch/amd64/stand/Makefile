#	$OpenBSD: Makefile,v 1.8 2025/11/12 11:34:36 hshoexer Exp $

.if ${MACHINE} == "amd64"
SUBDIR=	mbr cdbr
.endif
SUBDIR+=biosboot boot cdboot fdboot pxeboot efiboot
SUBDIR+=rdboot vmboot

.include <bsd.subdir.mk>
