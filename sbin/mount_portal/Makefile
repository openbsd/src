#	$OpenBSD: Makefile,v 1.8 2001/07/04 06:32:18 deraadt Exp $

PROG=	mount_portal
SRCS=	mount_portal.c activate.c conf.c getmntopts.c pt_conf.c \
	pt_exec.c pt_file.c pt_tcp.c
MAN=	mount_portal.8
MLINKS=	mount_portal.8 portal.conf.5

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>

afterinstall:
	if [ ! -f ${DESTDIR}/etc/portal.conf ]; then \
		${INSTALL} ${INSTALL_COPY} -o root -g wheel -m 644 \
		    ${.CURDIR}/portal.conf ${DESTDIR}/etc; \
	fi
