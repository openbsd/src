#	$OpenBSD: Makefile,v 1.11 2002/06/12 19:42:22 niklas Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= crypto ddb fdescfs kern sys uvm
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
