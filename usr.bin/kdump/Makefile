#	$OpenBSD: Makefile,v 1.4 2002/10/17 15:14:57 jason Exp $

LDSTATIC=${STATIC}
PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c

ioctl.c: mkioctls
	/bin/sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include/sys/ioctl.h \
	    ${DESTDIR}/usr/include/sys/ioctl_compat.h \
	    ${DESTDIR}/usr/include/sys/audioio.h > ioctl.c

.include <bsd.prog.mk>
