#	$OpenBSD: Makefile,v 1.15 2004/04/24 13:19:12 miod Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR+= crypto ddb fdescfs fifofs kern ptrace sys uvm dev
.if exists(arch/${MACHINE})
SUBDIR+= arch/${MACHINE}
.endif
.if exists(arch/${MACHINE_ARCH}) && ${MACHINE} != ${MACHINE_ARCH}
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
