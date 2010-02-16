
REV=	${OSrev}
BSD_RD=		bsd.rd

IMAGE=	miniroot${REV}.fs

MOUNT_POINT=	/mnt

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_CDEV=	/dev/${VND}c
VND_IDEV=	/dev/${VND}i
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
VND_RIDEV=	/dev/r${VND}i
PID!=		echo $$$$

DISKTYPE=       miniroot
# if we make the image smaller, there is no room for an alternate sb
# out fsck_ext2fs does not like that
NBLKS=		18432


.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else
all:	${IMAGE}

${IMAGE}: rd_setup do_files rd_teardown

.endif

do_files:
	cp ${.OBJDIR}/../ramdisk/bsd.rd ${MOUNT_POINT}/bsd.rd

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c -t ${DISKTYPE} ${VND} ${IMAGE}
	echo 'u\ne 0\n83\n\n63\n*\nw\nq\n' | fdisk -e ${VND} > /dev/null
	echo 'w\ny\nq\n' | disklabel -E ${VND} > /dev/null
	newfs_ext2fs ${VND_RIDEV}
	fsck_ext2fs -f ${VND_RIDEV}
	mount ${VND_IDEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}


unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

.PRECIOUS:	${IMAGE}

.ifdef RELEASEDIR
install:
	cp ${IMAGE} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
