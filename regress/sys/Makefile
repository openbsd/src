#	$OpenBSD: Makefile,v 1.9 2002/03/25 10:56:58 fgsch Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= kern fdescfs uvm crypto
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
