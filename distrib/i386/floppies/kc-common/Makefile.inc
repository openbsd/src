#	$OpenBSD: Makefile.inc,v 1.3 1996/04/25 21:28:15 niklas Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		kcbin
COMMONDIR=	${TOP}/kc-common

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE?=		xxx-${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN}
.ifndef FLOPPY3
.else
.endif
.ifndef FLOPPY3
	dd if=/dev/zero of=${IMAGE} bs=10k count=120
	vnconfig -v -c ${VND_DEV} ${IMAGE}
	disklabel -w -B -b ${MDEC}/fdboot -s ${MDEC}/bootfd ${VND} floppy5
	newfs -O -m 0 -o space -i 5120 -c 80 ${VND_RDEV} floppy5
.else
	dd if=/dev/zero of=${IMAGE} bs=10k count=144
	vnconfig -v -c ${VND_DEV} ${IMAGE}
	disklabel -w -B -b ${MDEC}/fdboot -s ${MDEC}/bootfd ${VND} floppy3
	newfs -O -m 0 -o space -i 5120 -c 80 ${VND_RDEV} floppy3
.endif
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}
	cat /*bin/* > /dev/null

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
