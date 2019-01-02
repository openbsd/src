#	$OpenBSD: Makefile,v 1.20 2019/01/02 18:56:17 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso

FS=		install${REV}.fs
FSSIZE=		757760
FSTYPE=		install360

RAMDISK=	RAMDISK_CD

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BSDRD=		${RELDIR}/bsd.rd
BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/game${OSrev}.tgz ${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd ${RELDIR}/bsd.mp \
		${RELDIR}/INSTALL.${MACHINE}
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xfont${OSrev}.tgz \
		${RELXDIR}/xshare${OSrev}.tgz ${RELXDIR}/xserv${OSrev}.tgz

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
VND_IDEV=	/dev/${VND}i
VND_IRDEV=	/dev/r${VND}i
BOOT?=		${DESTDIR}/usr/mdec/boot
EFIBOOT?=	${DESTDIR}/usr/mdec/BOOTX64.EFI ${DESTDIR}/usr/mdec/BOOTIA32.EFI
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
MSDOSSIZE=	960
TOTALSIZE!=	expr ${FSSIZE} + ${MSDOSSIZE}
TEMPLATE=	${.CURDIR}/template

all: ${CDROM} ${FS}

${FS}: ${BASE} ${XBASE} bsd.gz
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${TOTALSIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	fdisk -yi -l ${FSSIZE} -b ${MSDOSSIZE} -f ${DESTDIR}/usr/mdec/mbr ${VND}
.ifdef TEMPLATE
	disklabel -wAT ${TEMPLATE} ${VND}
	newfs -t msdos ${VND_IRDEV}
	mount ${VND_IDEV} ${MOUNT_POINT}
	mkdir -p ${MOUNT_POINT}/efi/boot
	cp ${EFIBOOT} ${MOUNT_POINT}/efi/boot
	umount ${MOUNT_POINT}
.else
	disklabel -w ${VND} ${FSTYPE}
.endif
	newfs -m 0 -o space -i 524288 -c ${FSSIZE} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${BOOT} ${MOUNT_POINT}/boot
	strip ${MOUNT_POINT}/boot
	strip -R .comment -R .SUNW_ctf ${MOUNT_POINT}/boot
	dd if=bsd.gz of=${MOUNT_POINT}/bsd bs=512
	installboot -v -r ${MOUNT_POINT} ${VND_CRDEV} \
	    ${DESTDIR}/usr/mdec/biosboot ${MOUNT_POINT}/boot
	mkdir -p ${MOUNT_POINT}/${OSREV}/${MACHINE}
	mkdir -p ${MOUNT_POINT}/etc
	echo "set image /${OSREV}/${MACHINE}/bsd.rd" > ${MOUNT_POINT}/etc/boot.conf

	cp -p ${BASE} ${MOUNT_POINT}/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${MOUNT_POINT}/${OSREV}/${MACHINE}

	cat ${RELDIR}/SHA256 ${RELXDIR}/SHA256 > \
	    ${MOUNT_POINT}/${OSREV}/${MACHINE}/SHA256
	# XXX no SHA256.sig

	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${FS}
	rm ${REALIMAGE}

${CDROM}: ${BASE} ${XBASE}
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/${MACHINE}/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cat ${RELDIR}/SHA256 ${RELXDIR}/SHA256 > \
	    ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/SHA256
	# XXX no SHA256.sig

	cp -p ${RELDIR}/cdbr ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${RELDIR}/cdboot ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/cdboot

	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "OpenBSD ${OSREV} ${MACHINE} Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/${MACHINE}   ${OSREV} Install CD" \
	    -b ${OSREV}/${MACHINE}/cdbr -c ${OSREV}/${MACHINE}/boot.catalog \
	    ${.OBJDIR}/cd-dir

install:
	cp ${CDROM} ${FS} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM} ${FS}
	rm -rf cd-dir

bsd.gz: bsd.rd
	cp ${BSDRD} bsd.strip
	strip bsd.strip
	strip -R .comment -R .SUNW_ctf bsd.strip
	gzip -c9n bsd.strip > bsd.gz

bsd.rd: ${BSDRD}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
