
IMAGE=	miniroot${OSrev}.fs

MOUNT_POINT=	/mnt

DISKTYPE=	miniroot
NBLKS=		43008
MSDOSSTART=	8192
MSDOSSIZE=	8192
FFSSTART!=	expr ${MSDOSSTART} + ${MSDOSSIZE}

FS?=		msdos
PART_ID?=C

NEWFS_ARGS_msdos=-L boot -c1 -F16
MOUNT_ARGS_msdos=-o-l

PDTB=		/usr/local/share/dtb/arm64
PUBOOT=		/usr/local/share/u-boot
PRPI=		/usr/local/share/raspberrypi-firmware/boot

PIFILES=\
	bootcode.bin \
	start.elf \
	fixup.dat \
	bcm2710-rpi-3-b.dtb \
	bcm2710-rpi-3-b-plus.dtb \
	bcm2710-rpi-cm3.dtb

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
	cp ${.OBJDIR}/../ramdisk/bsd.rd ${MOUNT_POINT}/bsd.rd
	ln ${MOUNT_POINT}/bsd.rd ${MOUNT_POINT}/bsd
	-umount ${MOUNT_POINT}
	mount ${MOUNT_ARGS_msdos} /dev/`cat vnd`i ${MOUNT_POINT}
.for FILE in ${PIFILES}
	cp ${PRPI}/${FILE} ${MOUNT_POINT}/
.endfor
	cp ${PUBOOT}/rpi_3/u-boot.bin ${MOUNT_POINT}/
	mkdir -p ${MOUNT_POINT}/efi/boot
	cp /usr/mdec/BOOTAA64.EFI ${MOUNT_POINT}/efi/boot/bootaa64.efi
	echo bootaa64.efi > ${MOUNT_POINT}/efi/boot/startup.nsh
	echo 'arm_64bit=1\nenable_uart=1\ndevice_tree_address=0x02600000\nkernel=u-boot.bin' > ${MOUNT_POINT}/config.txt
	dd if=${PUBOOT}/pine64_plus/u-boot-sunxi-with-spl.bin \
	    of=/dev/r`cat vnd`c bs=1024 seek=8

rd_setup:
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -t ${DISKTYPE} ${IMAGE} > vnd
	echo 'u\ne 0\n${PART_ID}\nn\n${MSDOSSTART}\n${MSDOSSIZE}\ne 3\nA6\nn\n${FFSSTART}\n*\nf 0\nw\nq\n' \
	    | fdisk -e `cat vnd` >/dev/null
	echo 'a a\n\n\n\nw\nq\n' | disklabel -E `cat vnd` >/dev/null
	newfs -t msdos ${NEWFS_ARGS_msdos} /dev/r`cat vnd`i
	newfs -m 0 -o space -i 4096 /dev/r`cat vnd`a
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
.include <bsd.own.mk>
