#	$OpenBSD: Makefile,v 1.8 2013/05/12 10:43:45 miod Exp $

.if ${MACHINE} == "mvme88k"
SUBDIR=  bugcrt libbug libsa libz
SUBDIR+= bootxx bootsd wrtvid bootst
SUBDIR+= netboot sboot tftpboot
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
