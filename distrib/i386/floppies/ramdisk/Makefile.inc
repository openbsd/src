#
#	$OpenBSD: Makefile.inc,v 1.5 1997/05/04 03:05:40 tholo Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN?=		instbin
COMMONDIR=	${TOP}/ramdisk

MOUNT_POINT?=	${TOP}/${BASE}/fs

LISTS=		${COMMONDIR}/../common/list ${.CURDIR}/list
CRUNCHCONF?=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c \
	    bsd bsd.rd bsd.gz floppy-*.fs 

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
