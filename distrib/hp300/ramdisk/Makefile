#	$OpenBSD: Makefile,v 1.3 1997/04/21 07:32:11 downsj Exp $
#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk-${REV}.fs
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
BASE=miniroot

MOUNT_POINT=	/mnt

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}

all: ${CBIN} ${IMAGE}
	@echo "all done"

.include "Makefile.inc"

DISKTYPE=       rdroot
NBLKS=          4096
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -c 16 -i 4096

bsd.gz: bsd.rd
	gzip -c9 bsd.rd > bsd.gz

bsd.rd: ${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}

bsd:
#	cd ${.CURDIR}/../../../sys/arch/hp300/conf && config RAMDISK
#	cd ${.CURDIR}/../../../sys/arch/hp300/compile/RAMDISK && \
#	    make clean && make depend && make
	cp ${.CURDIR}/../../../sys/arch/hp300/compile/RAMDISK/bsd bsd

${IMAGE}: ${CBIN} rd_setup do_files rd_teardown

rd_setup: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV} ${DISKTYPE}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	sync
	cat /*bin/* > /dev/null
	sync
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

rdsetroot:	${TOP}/../common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/../common/rdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.PRECIOUS:	${IMAGE}


