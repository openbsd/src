#	$OpenBSD: Makefile,v 1.7 2004/01/28 08:46:29 miod Exp $

.if ${MACHINE} == "mvme88k"
SUBDIR=  bugcrt libbug libsa libz wrtvid bootsd bootxx bootst netboot tftpboot
# not really written yet...
# SUBDIR+=	sboot
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
