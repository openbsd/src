#	$OpenBSD: Makefile,v 1.1 1997/05/07 12:46:54 niklas Exp $

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

.include "${TOP}/inst-common/Makefile.inc"

all: ${CBIN} ${IMAGE}
	@echo "all done"

DISKTYPE=       rdroot
NBLKS=          5120
# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -t ffs -m 0 -o space -c 16 -i 16384

setroot: ${IMAGE} bsd ecoffrdsetroot
	${.OBJDIR}/ecoffrdsetroot bsd < ${IMAGE}

${IMAGE}:	do_prep do_mount do_files do_umount unconfig
	mv -f ${REALIMAGE} ${IMAGE}

ecoffrdsetroot:	${TOP}/ecoffrdsetroot.c
	${HOSTCC} -o ecoffrdsetroot ${TOP}/ecoffrdsetroot.c

bsd:	${KERNEL}
	cp ${KERNEL} bsd

do_prep: ${CBIN} unconfig
	dd if=/dev/zero of=${REALIMAGE} count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	
do_mount:
	-newfs ${NEWFSARGS} -s ${NBLKS} ${VND_RDEV} ${DISKTYPE}
	mount ${VND_DEV} ${MOUNT_POINT}
 
do_umount:
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	-umount ${MOUNT_POINT}

unconfig:
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND} && cat /*bin/* > /dev/null
	-rm -f ${REALIMAGE}

.PRECIOUS:	${IMAGE}
