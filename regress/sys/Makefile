#	$OpenBSD: Makefile,v 1.6 2002/02/23 01:25:11 art Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= kern fdescfs uvm
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
