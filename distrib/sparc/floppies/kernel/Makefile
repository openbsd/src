#	$OpenBSD: Makefile,v 1.12 1997/09/26 01:51:30 millert Exp $

.include "../Makefile.inc"

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
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
	newfs -O -m 0 -o space -i 20480 -c 80 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	sync; /usr/mdec/binstall -v ffs ${MOUNT_POINT}
	TOPDIR=${.CURDIR}/.. CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${.CURDIR}/../runlist.sh ${LISTS}
	mtree -def ${.CURDIR}/mtree.conf -p ${MOUNT_POINT}/ -u
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

clean cleandir:
	/bin/rm -f ${IMAGE} bsd

beforeinstall:
	cp ${IMAGE} ${DESTDIR}/snapshot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
