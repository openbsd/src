#	$OpenBSD: Makefile,v 1.20 2015/10/14 12:47:07 vgross Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= copy crypto ddb fifofs kern net netinet netinet6 ptrace sys uvm dev
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif
.if exists(arch/${MACHINE_ARCH}) && ${MACHINE} != ${MACHINE_ARCH}
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
