#	$OpenBSD: Makefile,v 1.4 1997/05/08 05:44:58 millert Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs
CRUNCHCONF?=    ${TOP}/ramdisk/${CBIN}.conf
BASE=		ramdisk
KERNEL=		${TOP}/../../sys/arch/${MACHINE}/compile/RAMDISK/bsd

MOUNT_POINT=	/mnt

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}

STRIP?=		strip
GZIP?=		gzip
GZIPFLAGS?=	-9f
INSTALLBOOT?=	/usr/mdec/installboot -v

FLOPPY?=	floppy.fs
FLOPPYBLKS?=	2880
FLOPPYTYPE?=	floppy3
BOOT?=		/usr/mdec/boot
BOOTXX?=	/usr/mdec/bootxx

.include "${TOP}/inst-common/Makefile.inc"

all: ${CBIN} ${FLOPPY}
	@echo "all done"

DISKTYPE=       rdroot
NBLKS=          8192

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS=	-t ffs -m 0 -o space
NEWFSARGS_RD=	-i 16384 -c 16
NEWFSARGS_FD=	-i 524288 -c 80

${FLOPPY}:	bsd.gz
	dd if=/dev/zero of=${REALIMAGE} count=${FLOPPYBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${FLOPPYTYPE}
	newfs ${NEWFSARGS} ${NEWFSARGS_FD} -s ${FLOPPYBLKS} ${VND_RDEV} \
	    ${FLOPPYTYPE}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp bsd.gz ${MOUNT_POINT}/bsd
	cp ${BOOT} ${MOUNT_POINT}
	${INSTALLBOOT} ${MOUNT_POINT}/boot ${BOOTXX} ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cat /*bin/* > /dev/null
	# some bug can put an extra 8k at the end it seems
	dd if=${REALIMAGE} of=${FLOPPY} count=${FLOPPYBLKS}

bsd.gz: ${KERNEL} ${IMAGE} ecoffrdsetroot
	cp ${KERNEL} bsd
	${.OBJDIR}/ecoffrdsetroot bsd < ${IMAGE}
	${STRIP} bsd
	${GZIP} ${GZIPFLAGS} bsd

${IMAGE}:	do_prep do_mount do_files do_umount
	mv -f ${REALIMAGE} ${IMAGE}

ecoffrdsetroot:	${TOP}/ecoffrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o ecoffrdsetroot ${TOP}/ecoffrdsetroot.c

bsd:	${KERNEL}
	cp ${KERNEL} bsd

do_prep:
	dd if=/dev/zero of=${REALIMAGE} count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	
do_mount:
	newfs ${NEWFSARGS} ${NEWFSARGS_RD} -s ${NBLKS} ${VND_RDEV} ${DISKTYPE}
	mount ${VND_DEV} ${MOUNT_POINT}
 
do_umount:
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cat /*bin/* > /dev/null

unconfig:
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND} && cat /*bin/* > /dev/null
