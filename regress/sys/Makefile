#	$OpenBSD: Makefile,v 1.25 2017/05/29 11:59:32 sf Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR=	copy crypto dev ddb ffs fifofs fileops kern \
	net netinet netinet6 nfs ptrace sys uvm
.if exists(arch/${MACHINE}/Makefile)
SUBDIR+= arch/${MACHINE}
.endif
.if exists(arch/${MACHINE_ARCH}/Makefile) && ${MACHINE} != ${MACHINE_ARCH}
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
