#	$OpenBSD: Makefile,v 1.5 2002/02/16 17:38:31 art Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= kern fdescfs uvm
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif

regress: _SUBDIRUSE

install:

.include <bsd.subdir.mk>
