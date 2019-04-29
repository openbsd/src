
IMAGE=	miniroot${OSrev}.fs

MOUNT_POINT=	/mnt

DISKTYPE=       miniroot
NBLKS=		24576
NEWFSARGS= -t msdos


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
	echo 'u\ne 0\nC\nn\n64\n22528\nf 0\nw\nq\n' | fdisk -e `cat vnd`
	echo 'w\ny\nq\n' | disklabel -E `cat vnd` > /dev/null
	newfs ${NEWFSARGS} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}

rd_teardown:
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
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
