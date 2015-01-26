
REV=	${OSrev}
BSD_RD=		bsd.rd.OMAP

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
.if ${PLATFORM} == "OMAP"
	cp /usr/mdec/${BOARD}/mlo ${MOUNT_POINT}/mlo
	echo "bootcmd=mmc rescan ; setenv loadaddr ${LOADADDR} ; setenv bootargs sd0i:/bsd.umg ; fatload mmc \$${mmcdev} \$${loadaddr} bsd.umg ; bootm \$${loadaddr} ;\nuenvcmd=boot" > ${MOUNT_POINT}/uenv.txt
	cp /usr/mdec/${BOARD}/u-boot.* ${MOUNT_POINT}/
	mkdir ${MOUNT_POINT}/u-boots
	cp -r /usr/mdec/{am335x,beagle,panda} ${MOUNT_POINT}/u-boots
.endif
.if ${PLATFORM} == "IMX"
	echo "; setenv loadaddr ${LOADADDR} ; setenv bootargs sd0i:/bsd.umg ; for dtype in sata mmc ; do for disk in 0 1 ; do \$${dtype} dev \$${disk} ; for fs in fat ext2 ; do if \$${fs}load \$${dtype} \$${disk}:1 \$${loadaddr} bsd.umg ; then  bootm \$${loadaddr} ; fi ; done; done; done; echo; echo failed to load bsd.umg" > 6x_bootscript.cmd
	${MKUBOOT} -t script -a arm -o linux 6x_bootscript.cmd 6x_bootscript.scr
	cp 6x_bootscript.scr ${MOUNT_POINT}/6x_bootscript

.endif
.if ${PLATFORM} == "SUNXI"
	echo 'bootargs=sd0a:/bsd' > ${MOUNT_POINT}/uEnv.tx
	echo 'mmcboot=mmc rescan ; fatload mmc 0 ${LOADADDR} bsd.umg && bootm ${LOADADDR};' >> ${MOUNT_POINT}/uEnv.txt
	echo 'uenvcmd=run mmcboot;' >> ${MOUNT_POINT}/uEnv.txt
.endif
	cp ${.OBJDIR}/../../ramdisk/bsd.rd.${PLATFORM}.umg ${MOUNT_POINT}/bsd.umg

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -c ${VND} ${IMAGE}
	fdisk -c 2 -h 255 -s 63 -yi ${VND} >/dev/null
.if ${PLATFORM} == "SUNXI"
	echo "u\ne 0\n${PART_ID}\ny\n0\n99\n1\n1\n254\n63\nf 0\nw\nq\n" | fdisk -c 2 -h 255 -s 63 -e ${VND} >/dev/null
.else
	echo "u\ne 0\n${PART_ID}\ny\n0\n1\n1\n1\n254\n63\nf 0\nw\nq\n" | fdisk -c 2 -h 255 -s 63 -e ${VND} >/dev/null
.endif
	newfs_${FS} ${NEWFS_ARGS_${FS}} ${VND_RIDEV} >/dev/null
	mount ${VND_IDEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
.if ${PLATFORM} == "SUNXI"
	dd if=/usr/mdec/${BOARD}/u-boot-sunxi-with-spl.bin \
	    of=${VND_CDEV} bs=1024 seek=8
.endif
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
