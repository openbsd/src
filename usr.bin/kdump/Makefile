#	$OpenBSD: Makefile,v 1.6 2002/12/04 17:41:00 mickey Exp $

LDSTATIC=${STATIC}
PROG=	kdump
CFLAGS+=-I${.CURDIR}/../ktrace
SRCS=	kdump.c ioctl.c subr.c
.PATH:	${.CURDIR}/../ktrace
CLEANFILES+=ioctl.c

ioctl.c: mkioctls
	/bin/sh ${.CURDIR}/mkioctls ${DESTDIR}/usr/include/sys/ioctl.h \
	    ${DESTDIR}/usr/include/sys/ioctl_compat.h \
	    ${DESTDIR}/usr/include/dev/wscons/wsconsio.h \
	    ${DESTDIR}/usr/include/dev/systrace.h \
	    ${DESTDIR}/usr/include/sys/ataio.h \
	    ${DESTDIR}/usr/include/sys/audioio.h \
	    ${DESTDIR}/usr/include/sys/cdio.h \
	    ${DESTDIR}/usr/include/sys/chio.h \
	    ${DESTDIR}/usr/include/sys/dkio.h \
	    ${DESTDIR}/usr/include/sys/filio.h \
	    ${DESTDIR}/usr/include/sys/midiio.h \
	    ${DESTDIR}/usr/include/sys/mtio.h \
	    ${DESTDIR}/usr/include/sys/pciio.h \
	    ${DESTDIR}/usr/include/sys/radioio.h \
	    ${DESTDIR}/usr/include/sys/scanio.h \
	    ${DESTDIR}/usr/include/sys/scsiio.h \
	    ${DESTDIR}/usr/include/sys/sockio.h > ioctl.c

.include <bsd.prog.mk>
