#	$OpenBSD: Makefile,v 1.13 2004/01/29 22:09:57 millert Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= crypto ddb fdescfs fifofs kern sys uvm dev
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
