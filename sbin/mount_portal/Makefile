#	$OpenBSD: Makefile,v 1.2 1996/06/23 14:31:30 deraadt Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/18 14:57:50 cgd Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c
MAN=	mount_portal.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	$(LIBCOMPAT)
LDADD=	-lcompat

.include <bsd.prog.mk>
