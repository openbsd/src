
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
PID!=		echo $$$$

DISKTYPE=       miniroot
NBLKS=		8064
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -i 4096


.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else
all:	${IMAGE}

${IMAGE}: rd_setup do_files rd_teardown

.endif

do_files:
	dd if=${DESTDIR}/usr/mdec/boot.elf of=${VND_IDEV}
	gzip -c < ${.OBJDIR}/../ramdisk/bsd.rd > ${MOUNT_POINT}/bsd.rd
	ln ${MOUNT_POINT}/bsd.rd ${MOUNT_POINT}/bsd

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c -t ${DISKTYPE} ${VND} ${IMAGE}
	fdisk -i -y ${VND}
	disklabel -w ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

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
