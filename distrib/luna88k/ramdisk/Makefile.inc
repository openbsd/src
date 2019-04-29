#	$OpenBSD: Makefile.inc,v 1.10 2019/04/29 15:56:26 deraadt Exp $

LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

COMMONDIR=	${.CURDIR}/../ramdisk
MOUNT_POINT?=	${.CURDIR}/../${BASE}/fs

CRUNCHCONF?=	${COMMONDIR}/instbin.conf
MTREE=		${UTILS}/mtree.conf

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk all

#instbin: instbin.mk instbin.cache instbin.c
#	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    OSrev=${OSrev} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/instbin

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist instbin.map

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
