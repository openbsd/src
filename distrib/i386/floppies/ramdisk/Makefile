#	$OpenBSD: Makefile,v 1.13 1997/05/07 04:46:52 deraadt Exp $

# Revision is 2.1
REV=           21

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
BASE=mr

MOUNT_POINT=	/mnt

FS=		floppy.fs
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}
LISTFLOPPY=	${.CURDIR}/list_floppy

.include "Makefile.inc"

all:	${FS}

${FS}:	bsd.gz
.ifndef FLOPPY3
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=120
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} floppy5
	newfs -m 0 -o space -i 8192 -c 80 ${VND_RDEV} floppy5
.else
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} floppy3
	newfs -m 0 -o space -i 32768 -c 80 ${VND_RDEV} floppy3
.endif
	mount ${VND_DEV} ${MOUNT_POINT}
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTFLOPPY}
	sync
	cat /*bin/* > /dev/null
	sync
	/usr/mdec/installboot -v /mnt/boot ${DESTDIR}/usr/mdec/biosboot ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	sync
	cat /*bin/* > /dev/null
	sync
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${FS}
	rm ${REALIMAGE}

DISKTYPE=       rdroot
NBLKS=          4046
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -c 16 -i 4096

bsd.gz: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	gzip -c9 bsd.strip > bsd.gz

bsd.rd:	${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../../sys/arch/i386/conf && config RAMDISK
	cd ${.CURDIR}/../../../../sys/arch/i386/compile/RAMDISK && \
	    make clean && make depend && make
	cp ${.CURDIR}/../../../../sys/arch/i386/compile/RAMDISK/bsd bsd

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

rdsetroot:	${TOP}/common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/common/rdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.PRECIOUS:	${IMAGE}


install:
	cp ${FS} ${DESTDIR}/snapshot/${FS}
