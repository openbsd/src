#
#	$OpenBSD: Makefile.inc,v 1.9 2009/04/17 03:58:54 deraadt Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN?=		instbin
COMMONDIR=	${TOP}/inst-common
UTILS=		${TOP}/../miniroot

MOUNT_POINT?=	${TOP}/${BASE}/fs

LISTS?=		${.CURDIR}/../common/list
CRUNCHCONF?=	${CBIN}.conf
MTREE=		${UTILS}/mtree.conf
STRIP=		strip

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all
	${STRIP} -R .eh_frame ${CBIN}

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS}\
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
