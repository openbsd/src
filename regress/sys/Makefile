#	$OpenBSD: Makefile,v 1.12 2003/07/31 15:56:33 deraadt Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= crypto ddb fdescfs kern sys uvm dev
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
