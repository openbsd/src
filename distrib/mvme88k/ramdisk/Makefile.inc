#
#	$OpenBSD: Makefile.inc,v 1.3 2002/04/28 18:28:34 deraadt Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN?=		raminst
COMMONDIR=	${TOP}/ramdisk
UTILS?=		${TOP}/../miniroot

MOUNT_POINT?=	${TOP}/${BASE}/fs

LISTS=		${.CURDIR}/list
CRUNCHCONF?=	${COMMONDIR}/${CBIN}.conf
MTREE=		${UTILS}/mtree.conf
RAWLABEL=	

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -U 
	@echo mtree done...
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    sh ${UTILS}/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
