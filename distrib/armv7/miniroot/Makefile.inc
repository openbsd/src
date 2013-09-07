
REV=	${OSrev}
BSD_RD=		bsd.rd.OMAP

IMAGE=	miniroot-${BOARD}-${REV}.fs

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


.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else
all:	${IMAGE}

${IMAGE}: rd_setup do_files rd_teardown

.endif

do_files:
	echo 'bootcmd=mmc rescan ; setenv loadaddr 0x82800000 ; setenv bootargs sd0i:/bsd.umg ; fatload mmc 0 $${loadaddr} bsd.umg ; bootm $${loadaddr} ;\nuenvcmd=boot' > ${MOUNT_POINT}/uenv.txt
	cp /usr/mdec/${BOARD}/mlo ${MOUNT_POINT}/mlo
	cp /usr/mdec/${BOARD}/u-boot.* ${MOUNT_POINT}/
	mkdir ${MOUNT_POINT}/u-boots
	cp -r /usr/mdec/{am335x,beagle,panda} ${MOUNT_POINT}/u-boots
	mkuboot -a arm -o linux -e 0x80300000 -l 0x80300000 \
		${.OBJDIR}/../../ramdisk/bsd.rd.OMAP ${MOUNT_POINT}/bsd.umg

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -c ${VND} ${IMAGE}
	fdisk -c 2 -h 255 -s 63 -yi ${VND} >/dev/null
	echo 'u\ne 0\nC\ny\n0\n1\n1\n1\n254\n63\nf 0\nw\nq\n' | fdisk -c 2 -h 255 -s 63 -e ${VND} >/dev/null
	newfs_msdos -F 16 -L boot ${VND_RIDEV} >/dev/null
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
.include <bsd.own.mk>
