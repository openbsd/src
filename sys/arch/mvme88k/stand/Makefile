#	$OpenBSD: Makefile,v 1.6 2004/01/26 19:48:31 miod Exp $
.if ${MACHINE} == "mvme88k"

SUBDIR=  bugcrt libbug libsa libz wrtvid bootsd bootxx bootst netboot
# not ready yet
# SUBDIR+=	tftpboot
# not really written yet...
# SUBDIR+=	sboot
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
