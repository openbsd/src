#	$OpenBSD: Makefile,v 1.3 1996/09/16 10:16:57 downsj Exp $

LDSTATIC=${STATIC}
PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c

ioctl.c: mkioctls
	/bin/sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include/sys/ioctl.h \
	    ${DESTDIR}/usr/include/sys/ioctl_compat.h > ioctl.c

.include <bsd.prog.mk>
