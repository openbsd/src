#	$OpenBSD: Makefile,v 1.6 1997/04/27 20:56:22 millert Exp $
#	$NetBSD: Makefile,v 1.10 1995/03/18 14:57:50 cgd Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c
MAN=	mount_portal.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

afterinstall:
	if [ ! -f ${DESTDIR}/etc/portal.conf ]; then \
		${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 644 \
		    ${.CURDIR}/portal.conf ${DESTDIR}/etc; \
	fi
