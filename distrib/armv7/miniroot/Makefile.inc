
REV=	${OSrev}

IMAGE=	miniroot-${BOARD}-${REV}.fs
MKUBOOT?=	mkuboot

MOUNT_POINT=	/mnt

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_CDEV=	/dev/${VND}c
VND_IDEV=	/dev/${VND}i
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
VND_RIDEV=	/dev/r${VND}i
PID!=		echo $$$$

NBLKS=		36864

FS?=		msdos
PART_ID?=C

NEWFS_ARGS_msdos=-F 16 -L boot
NEWFS_ARGS_ext2fs=-v boot
MOUNT_ARGS_msdos=-o-l
MOUNT_ARGS_ext2fs=

cleandir: clean
clean:
	rm -f ${IMAGE}

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else
all:	${IMAGE}

${IMAGE}: rd_setup do_files rd_teardown

.endif

do_files:
	${MKUBOOT} -t script -a arm -o linux \
	    ${.CURDIR}/../boot.cmd ${MOUNT_POINT}/boot.scr
.if ${PLATFORM} == "OMAP"
	cp /usr/mdec/${BOARD}/MLO ${MOUNT_POINT}/MLO
	cp /usr/mdec/${BOARD}/u-boot.* ${MOUNT_POINT}/
	cp /usr/mdec/${BOARD}/*.dtb ${MOUNT_POINT}/
.endif
.if ${BOARD} == "nitrogen"
	mv ${MOUNT_POINT}/boot.scr ${MOUNT_POINT}/6x_bootscript
.endif
.if ${BOARD} == "cubox" || ${BOARD} == "wandboard"
	cp /usr/mdec/${BOARD}/*.dtb ${MOUNT_POINT}/
	dd if=/usr/mdec/${BOARD}/SPL of=${VND_CDEV} bs=1024 seek=1
	dd if=/usr/mdec/${BOARD}/u-boot.img of=${VND_CDEV} bs=1024 seek=69
.endif
.if ${PLATFORM} == "SUNXI"
	cp /usr/mdec/${BOARD}/*.dtb ${MOUNT_POINT}/
	dd if=/usr/mdec/${BOARD}/u-boot-sunxi-with-spl.bin \
	    of=${VND_CDEV} bs=1024 seek=8
.endif
	cp ${.OBJDIR}/../../ramdisk/bsd.rd.${PLATFORM}.umg ${MOUNT_POINT}/bsd.umg

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -c ${VND} ${IMAGE}
	fdisk -c 2 -h 255 -s 63 -yi ${VND} >/dev/null
.if ${PLATFORM} == "SUNXI"
	echo "u\ne 0\n${PART_ID}\ny\n0\n99\n1\n1\n254\n63\nf 0\nw\nq\n" \
	    | fdisk -c 2 -h 255 -s 63 -e ${VND} >/dev/null
.elif ${BOARD} == "cubox" || ${BOARD} == "wandboard"
	echo "u\ne 3\n0\ne 0\n${PART_ID}\ny\n0\n32\n33\n1\n254\n63\n63\nf 0\nw\nq" \
	    | fdisk -c 2 -h 255 -s 63 -e ${VND} >/dev/null
.else
	echo "u\ne 0\n${PART_ID}\ny\n0\n1\n1\n1\n254\n63\nf 0\nw\nq\n" \
	    | fdisk -c 2 -h 255 -s 63 -e ${VND} >/dev/null
.endif
	newfs_${FS} ${NEWFS_ARGS_${FS}} ${VND_RIDEV} >/dev/null
	mount ${MOUNT_ARGS_${FS}} ${VND_IDEV} ${MOUNT_POINT}

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
.include <bsd.own.mk>
