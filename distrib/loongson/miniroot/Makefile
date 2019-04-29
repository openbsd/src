
IMAGE=	miniroot${OSrev}.fs

MOUNT_POINT=	/mnt

DISKTYPE=       miniroot
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
	vnconfig -v -t ${DISKTYPE} ${IMAGE} > vnd
	echo 'u\ne 0\n83\n\n63\n*\nw\nq\n' | fdisk -e `cat vnd` > /dev/null
	echo 'w\ny\nq\n' | disklabel -E `cat vnd` > /dev/null
	newfs_ext2fs /dev/r`cat vnd`i
	mount /dev/`cat vnd`i ${MOUNT_POINT}

rd_teardown:
	df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.PRECIOUS:	${IMAGE}

.ifdef RELEASEDIR
install:
	cp ${IMAGE} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
