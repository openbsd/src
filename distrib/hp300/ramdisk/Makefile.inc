#
#	$OpenBSD: Makefile.inc,v 1.8 2009/04/17 03:58:54 deraadt Exp $
#	$NetBSD: Makefile.inc,v 1.1 1995/07/18 04:13:14 briggs Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN?=		instbin
COMMONDIR=	${TOP}/ramdisk
UTILS?=		${TOP}/../miniroot

MOUNT_POINT?=	${TOP}/${BASE}/fs

LISTS=		${.CURDIR}/list
CRUNCHCONF?=	${CBIN}.conf
MTREE=		${UTILS}/mtree.conf

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
