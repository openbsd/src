#	$OpenBSD: Makefile,v 1.15 1998/07/01 01:58:05 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs
CRUNCHCONF?=    ${TOP}/ramdisk/${CBIN}.conf
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
INSTALLBOOT?=	/usr/mdec/installboot -v

FLOPPY?=	floppy${REV}.fs
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
	newfs ${NEWFSARGS} ${NEWFSARGS_FD} -s ${FLOPPYBLKS} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${BOOT} ${MOUNT_POINT}
	cp bsd.gz ${MOUNT_POINT}/bsd
	${INSTALLBOOT} ${MOUNT_POINT}/boot ${BOOTXX} ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	@cmp ${BOOT} ${MOUNT_POINT}/boot || echo "Warning! /boot corrupted!!!"
	@cmp bsd.gz ${MOUNT_POINT}/bsd || echo "Warning! /bsd corrupted!!!"
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	if test `env BLOCKSIZE=512 du -s ${REALIMAGE} | cut -f1` -eq ${FLOPPYBLKS}; then \
	    mv ${REALIMAGE} ${FLOPPY} ; \
	else \
	    dd if=${REALIMAGE} of=${FLOPPY} count=${FLOPPYBLKS} ; \
	    rm -f ${REALIMAGE} ; \
	fi

bsd.gz:	bsd.rd
	cp bsd.rd bsd.strip
	${STRIP} bsd.strip
	${GZIP} -c ${GZIPFLAGS} bsd.strip > bsd.gz

bsd.rd: bsd ${IMAGE} ecoffrdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/ecoffrdsetroot bsd.rd < ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/conf && config RAMDISK
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/RAMDISK && \
	    make clean && make COPTS=-O
	cp ${.CURDIR}/../../../sys/arch/alpha/compile/RAMDISK/bsd bsd

${IMAGE}:	do_prep do_mount do_files do_umount
	mv -f ${REALIMAGE} ${IMAGE}

ecoffrdsetroot:	${TOP}/ecoffrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o ecoffrdsetroot ${TOP}/ecoffrdsetroot.c

do_prep:
	dd if=/dev/zero of=${REALIMAGE} count=${NBLKS}
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
	cp ${FLOPPY} bsd.rd ${DESTDIR}/snapshot
