#	$Id: Makefile,v 1.1 1995/10/27 22:40:01 deraadt Exp $

.include "../Makefile.inc"

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}c
VND_RDEV=	/dev/r${VND}c
IMAGE?=		xxx-${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${.CURDIR}/list

all:
	dd if=/dev/zero of=${IMAGE} bs=120k count=12
	vnconfig -v -c ${VND_DEV} ${IMAGE}
	-disklabel -w -r ${IMAGE} floppy
	newfs -O -m 0 -o space -i 6144 -c 80 ${VND_RDEV} floppy
	mount ${VND_DEV} ${MOUNT_POINT}
	TOPDIR=${.CURDIR}/.. CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${.CURDIR}/../runlist.sh ${LISTS}
	install -c -o root -g wheel -m 0440 ${MDEC}/boot ${MOUNT_POINT}/boot
	cat /bin/* > /dev/null		# flush buffer cache (yuck)
	installboot -v ${MOUNT_POINT}/boot ${MDEC}/bootxx ${IMAGE}
	mtree -def ${.CURDIR}/mtree.conf -p ${MOUNT_POINT}/ -u
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
	-/bin/rm -f ${IMAGE}

clean cleandir:
	/bin/rm -f ${IMAGE}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
