#	$OpenBSD: Makefile,v 1.6 2000/05/25 21:55:03 deraadt Exp $

.if ${MACHINE} == "mvme68k"
SUBDIR= bugcrt libbug libsa libz sboot netboot bootxx bootsd bootst \
	prtvid wrtvid
.endif

SUBDIR+=installboot

.include <bsd.subdir.mk>
