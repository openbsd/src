#
#	$NetBSD: Makefile.inc,v 1.1 1995/07/18 04:13:14 briggs Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN=		raminst
COMMONDIR=	${TOP}/inst-common

MOUNT_POINT?=	${TOP}/inst/fs

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN}
	rm -rf ${MOUNT_POINT}
	mkdir -p ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	mkcdfs ${IMAGE} ${MOUNT_POINT}

unconfig:
	-/bin/rm -f ${IMAGE}
	-/bin/rm -rf ${MOUNT_POINT}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c
	/bin/rm -rf ${MOUNT_POINT}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
