# $OpenBSD: Makefile.inc,v 1.3 1997/09/26 02:13:52 millert Exp $
# $NetBSD: Makefile.inc,v 1.1 1996/05/16 19:59:03 mark Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		instbin
COMMONDIR=	${TOP}/inst-common

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE?=		xxx-${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN}
	dd if=/dev/zero of=${IMAGE} bs=144k count=10
	vnconfig -v -c ${VND} ${IMAGE}
	disklabel -w ${VND} floppy
	newfs -O -m 0 -o space -i 5120 -c 80 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
	-/bin/rm -f ${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${TOP}/../../.. -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
