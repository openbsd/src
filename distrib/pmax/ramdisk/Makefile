#	$OpenBSD: Makefile,v 1.2 1998/11/16 23:39:15 millert Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk-${REV}.fs
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
BASE=		ramdisk

MOUNT_POINT=	/mnt

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}

STRIP?=		strip
GZIP?=		gzip
GZIPFLAGS?=	-9f

.include "Makefile.inc"

all: ${CBIN} ${IMAGE} bsd.rd
	@echo "all done"

DISKTYPE=       rdroot
NBLKS=          6144
NEWFSARGS= -m 0 -o space -c 16 -i 3072

bsd.rd: ${IMAGE} bsd.elf rdsetroot
	cp bsd.elf bsd.elf.rd
	${.OBJDIR}/rdsetroot bsd.elf.rd < ${IMAGE}
	elf2ecoff bsd.elf.rd bsd.rd
	rm bsd.elf.rd

bsd.elf:
	cd ${.CURDIR}/../../../sys/arch/pmax/conf && config RAMDISK
	cd ${.CURDIR}/../../../sys/arch/pmax/compile/RAMDISK && \
	    make clean && make
	cp ${.CURDIR}/../../../sys/arch/pmax/compile/RAMDISK/bsd.elf bsd.elf

${IMAGE}:	do_prep do_mount do_files do_umount
	mv -f ${REALIMAGE} ${IMAGE}

rdsetroot:	${TOP}/rdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot ${TOP}/rdsetroot.c

do_prep:
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	
do_mount:
	newfs ${NEWFSARGS} ${NEWFSARGS_RD} -s ${NBLKS} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
 
do_umount:
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}

unconfig:
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}

beforeinstall:
	cp bsd.rd ${DESTDIR}/snapshot
