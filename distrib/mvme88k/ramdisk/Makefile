#	$OpenBSDBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $
#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CRUNCHCONF?=	 ${.CURDIR}/${CBIN}.conf
BASE=mr

MOUNT_POINT=	/mnt

#FS=		ramdisk.fs
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}
LISTFLOPPY=	${.CURDIR}/list_ramdisk

.include "Makefile.inc"

all:	bsd.rd

#${FS}:	bsd.gz
#	dd if=/dev/zero of=${REALIMAGE} count=4096
#	vnconfig -v -c ${VND} ${REALIMAGE}
#	disklabel -w ${VND} rdroot
#	newfs -m 0 -o space -i 8192 -c 80 ${VND_RDEV}
#	mount ${VND_DEV} ${MOUNT_POINT}
#	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
#	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTFLOPPY}
#	/usr/mdec/installboot -v /mnt/boot ${DESTDIR}/usr/mdec/biosboot ${VND_CRDEV}
#	@echo ""
#	@df -i ${MOUNT_POINT}
#	@echo ""
#	umount ${MOUNT_POINT}
#	vnconfig -u ${VND}
#	cp ${REALIMAGE} ${FS}

DISKTYPE=	rdroot
NBLKS=		8192
# old format, minfree, opt, b/i  trks, sects, cpg
#NEWFSARGS= -t ffs -m 0 -o space -u 32 -c 16
#NEWFSARGS= -m 0 -o space -u 32 -c 16
NEWFSARGS=
bsd.gz: bsd.rd
	gzip -c9 bsd.rd > bsd.gz

bsd.rd: ${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.CURDIR}/${__objdir}/rdsetroot bsd.rd < ${IMAGE}

bsd:
#	cd ${.CURDIR}/../../../sys/arch/mvme88k/conf && config RAMDISK
#	cd ${.CURDIR}/../../../sys/arch/mvme88k/compile/RAMDISK && \
#	    make clean && make
	cp ${.CURDIR}/../../../sys/arch/mvme88k/compile/RAMDISK/bsd bsd

${IMAGE}: ${CBIN} rd_setup do_files rd_teardown

rd_setup: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	fsck -f -y ${VND_RDEV} > /dev/null
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

rdsetroot:	${TOP}/common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/common/rdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.PRECIOUS:      ${IMAGE}

install:
	cp bsd.rd ${DESTDIR}/snapshot/bsd.rd
