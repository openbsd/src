#
#	$OpenBSD: Makefile.inc,v 1.1 1998/12/17 02:16:31 smurph Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN?=		raminst
COMMONDIR=	${TOP}/ramdisk

MOUNT_POINT?=	${TOP}/${BASE}/fs

LISTS=		${.CURDIR}/list
CRUNCHCONF?=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf
RAWLABEL=	

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -U 
	@echo mtree done...
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
