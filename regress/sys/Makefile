#	$OpenBSD: Makefile,v 1.27 2022/10/22 17:50:28 gkoehler Exp $
#	$NetBSD: Makefile,v 1.4 1995/04/20 22:41:08 cgd Exp $

SUBDIR=	altivec_ast copy crypto dev ddb ffs \
	fifofs fileops kern mfs_noperm \
	net netinet netinet6 nfs ptrace sys uvm
.if exists(arch/${MACHINE}/Makefile)
SUBDIR+= arch/${MACHINE}
.endif
.if exists(arch/${MACHINE_ARCH}/Makefile) && ${MACHINE} != ${MACHINE_ARCH}
SUBDIR+= arch/${MACHINE_ARCH}
.endif

install:

.include <bsd.subdir.mk>
