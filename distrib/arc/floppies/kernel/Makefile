#	$OpenBSD: Makefile,v 1.5 1997/09/26 02:20:39 millert Exp $

.include "../Makefile.inc"

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}c
VND_RDEV=	/dev/r${VND}c
IMAGE=		kc${REV}.fs
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${.CURDIR}/list

bsd:
	cd ${.CURDIR}/../../../../sys/arch/sparc/conf && config FLOPPY
	cd ${.CURDIR}/../../../../sys/arch/sparc/compile/FLOPPY && \
	    make clean && make
	cp ${.CURDIR}/../../../../sys/arch/sparc/compile/FLOPPY/bsd bsd

all:	bsd
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} floppy
	newfs -O -m 0 -o space -i 8192 -c 80 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	TOPDIR=${.CURDIR}/.. CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${.CURDIR}/../runlist.sh ${LISTS}
	install -c -o root -g wheel -m 0440 ${MDEC}/boot ${MOUNT_POINT}/boot
	installboot -v ${MOUNT_POINT}/boot ${MDEC}/bootxx ${VND_RDEV}
	mtree -def ${.CURDIR}/mtree.conf -p ${MOUNT_POINT}/ -u
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
	-/bin/rm -f ${IMAGE}

clean cleandir:
	/bin/rm -f ${IMAGE}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
