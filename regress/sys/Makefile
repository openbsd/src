#	$OpenBSD: Makefile,v 1.8 2002/03/25 09:57:55 markus Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= kern fdescfs miscfs uvm crypto
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

install:

.include <bsd.subdir.mk>
