
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

DISKTYPE=	miniroot
NBLKS=		36864
MSDOSSTART=	2048
MSDOSSIZE=	4096
FFSSTART!=	expr ${MSDOSSTART} + ${MSDOSSIZE}

FS?=		msdos
PART_ID?=C

NEWFS_ARGS_msdos=-L boot
MOUNT_ARGS_msdos=-o-l

PDTB=		/usr/local/share/dtb/arm
PUBOOT=		/usr/local/share/u-boot

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
	cp ${.OBJDIR}/../../ramdisk/bsd.rd ${MOUNT_POINT}/bsd.rd
	ln ${MOUNT_POINT}/bsd.rd ${MOUNT_POINT}/bsd
	-umount ${MOUNT_POINT}
	mount ${MOUNT_ARGS_msdos} ${VND_IDEV} ${MOUNT_POINT}
.for DTB in ${DTBS}
	cp ${PDTB}/${DTB} ${MOUNT_POINT}/
.endfor
.if ${PLATFORM} == "OMAP"
	cp ${PUBOOT}/${UBOOT}/{MLO,u-boot.img} ${MOUNT_POINT}/
.endif
.if ${BOARD} == "cubox" || ${BOARD} == "wandboard"
	dd if=${PUBOOT}/${UBOOT}/SPL of=${VND_CDEV} bs=1024 seek=1
	dd if=${PUBOOT}/${UBOOT}/u-boot.img of=${VND_CDEV} bs=1024 seek=69
.endif
.if ${PLATFORM} == "SUNXI"
	dd if=${PUBOOT}/${UBOOT}/u-boot-sunxi-with-spl.bin \
	    of=${VND_CDEV} bs=1024 seek=8
.endif
	mkdir -p ${MOUNT_POINT}/efi/boot
	cp /usr/mdec/BOOTARM.EFI ${MOUNT_POINT}/efi/boot/bootarm.efi

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c -t ${DISKTYPE} ${VND} ${IMAGE}
	echo 'u\ne 0\n${PART_ID}\nn\n${MSDOSSTART}\n${MSDOSSIZE}\ne 3\nA6\nn\n${FFSSTART}\n*\nf 0\nw\nq\n' \
	    | fdisk -e ${VND} >/dev/null
	echo 'a a\n\n\n\nw\nq\n' | disklabel -E ${VND} >/dev/null
	newfs -t msdos ${NEWFS_ARGS_msdos} ${VND_RIDEV}
	newfs -m 0 -o space -i 4096 ${VND_RDEV}
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
.include <bsd.own.mk>
