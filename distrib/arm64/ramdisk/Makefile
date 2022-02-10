#	$OpenBSD: Makefile,v 1.24 2022/02/10 20:07:47 krw Exp $

FS=		miniroot${OSrev}.img
FSSIZE=		88064
MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf
RAMDISK=	RAMDISK

# this offset is required by Rockchip
MSDOSSTART=	32768
MSDOSSIZE=	16384

NEWFS_ARGS_msdos=-L boot -c1 -F16
MOUNT_ARGS_msdos=-o-l

PDTB=		/usr/local/share/dtb/arm64
PUBOOT=		/usr/local/share/u-boot
PRPI=		/usr/local/share/raspberrypi-firmware/boot

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

MRFSDISKTYPE=	rdroot
MRMAKEFSARGS=	-o disklabel=${MRFSDISKTYPE},minfree=0,density=4096

PIFILES=\
	bootcode.bin \
	start.elf \
	start4.elf \
	fixup.dat \
	fixup4.dat \
	bcm2710-rpi-2-b.dtb \
	bcm2710-rpi-3-b.dtb \
	bcm2710-rpi-3-b-plus.dtb \
	bcm2710-rpi-cm3.dtb \
	bcm2711-rpi-4-b.dtb \
	bcm2711-rpi-400.dtb \
	bcm2711-rpi-cm4.dtb

PIDTBO=\
	disable-bt.dtbo

all: ${FS}

${FS}: bsd.rd
	dd if=/dev/zero of=${FS} bs=512 count=${FSSIZE}
	vnconfig -v ${FS} > vnd
	fdisk -iy -b "${MSDOSSIZE}@${MSDOSSTART}:c" `cat vnd` >/dev/null
	echo 'a a\n\n\n\nw\nq\n' | disklabel -E `cat vnd` >/dev/null
	newfs -t msdos ${NEWFS_ARGS_msdos} /dev/r`cat vnd`i
	mount ${MOUNT_ARGS_msdos} /dev/`cat vnd`i ${MOUNT_POINT}
.for FILE in ${PIFILES}
	cp ${PRPI}/${FILE} ${MOUNT_POINT}/
.endfor
	mkdir -p ${MOUNT_POINT}/overlays
.for FILE in ${PIDTBO}
	cp ${PRPI}/overlays/${FILE} ${MOUNT_POINT}/overlays/
.endfor
	cp ${PUBOOT}/rpi_arm64/u-boot.bin ${MOUNT_POINT}/u-boot.bin
	mkdir -p ${MOUNT_POINT}/efi/boot
	cp /usr/mdec/BOOTAA64.EFI ${MOUNT_POINT}/efi/boot/bootaa64.efi
	echo bootaa64.efi > ${MOUNT_POINT}/efi/boot/startup.nsh
	echo 'arm_64bit=1\nenable_uart=1\ndtoverlay=disable-bt\nkernel=u-boot.bin' > ${MOUNT_POINT}/config.txt
	dd if=${PUBOOT}/pine64_plus/u-boot-sunxi-with-spl.bin \
	    of=/dev/r`cat vnd`c bs=1024 seek=8 conv=sync
	umount ${MOUNT_POINT}
	newfs -O 1 -m 0 -o space -i 524288 -c ${FSSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	install -c -m 555 -o root -g wheel bsd.rd ${MOUNT_POINT}/bsd
	ln ${MOUNT_POINT}/bsd ${MOUNT_POINT}/bsd.rd
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

bsd.rd: mr.fs bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd mr.fs

bsd:
	cd ${.CURDIR}/../../../sys/arch/${MACHINE}/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE} ${MFLAGS}'
	cp -p ${.CURDIR}/../../../sys/arch/${MACHINE}/compile/${RAMDISK}/obj/bsd bsd

mr.fs: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MRMAKEFSARGS} $@ $@.d

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib \
	    -c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} ${MFLAGS} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
install:
	cp bsd.rd ${RELEASEDIR}/bsd.rd
	chmod a+r ${RELEASEDIR}/bsd.rd
	cp ${FS} ${RELEASEDIR}
.endif

clean cleandir:
	rm -f *.core mr.fs instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map *.o *.lo *.c bsd.rd
	rm -f instbin.conf
	rm -rf cd-dir mr.fs.d

.include <bsd.obj.mk>
