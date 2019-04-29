
IMAGE=	miniroot${OSrev}.fs

MOUNT_POINT=	/mnt

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
	dd if=${DESTDIR}/usr/mdec/boot.elf of=/dev/r`cat vnd`i conv=osync
	gzip -c < ${.OBJDIR}/../ramdisk/bsd.rd > ${MOUNT_POINT}/bsd.rd
	ln ${MOUNT_POINT}/bsd.rd ${MOUNT_POINT}/bsd

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -t ${DISKTYPE} ${IMAGE} > vnd
	fdisk -i -y `cat vnd`
	disklabel -w `cat vnd` ${DISKTYPE}
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
