#	$OpenBSD: Makefile,v 1.19 2005/11/22 23:10:56 miod Exp $
#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk-${REV}.fs
CRUNCHCONF?=    ${CBIN}.conf
BASE=		ramdisk

MOUNT_POINT=	/mnt

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}

GZIP?=		gzip
GZIPFLAGS?=	-9f

all: ${CBIN} ${IMAGE} bsd.rd

install: bsd.rd
	${GZIP} -c ${GZIPFLAGS} bsd.rd > ${DESTDIR}/snapshot/bsd.rd

.include "Makefile.inc"

DISKTYPE=       rdroot
NBLKS=          4096
NEWFSARGS= -m 0 -o space -c 16 -i 3072

bsd.rd:
.ifndef(NOBUILD)
	cd ${.CURDIR}/../../../sys/arch/hp300/conf && config RAMDISK
	cd ${.CURDIR}/../../../sys/arch/hp300/compile/RAMDISK && \
	    make clean && make depend && COPTS=-Os make
.endif
	cp ${.CURDIR}/../../../sys/arch/hp300/compile/RAMDISK/bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}

${IMAGE}: ${CBIN} rd_setup do_files rd_teardown

rd_setup: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
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
