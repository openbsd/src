#	$OpenBSD: Makefile.inc,v 1.22 2011/04/15 03:11:22 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs

MOUNT_POINT=	/mnt

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}

STRIP?=		strip
GZIPCMD?=	gzip
GZIPFLAGS?=	-9fn
INSTALLBOOT?=	/usr/mdec/installboot -v

XNAME?=		floppy
FLOPPY?=	${XNAME}${REV}.fs
FLOPPYBLKS?=	2880
FLOPPYTYPE?=	floppy
BOOT?=		/usr/mdec/boot
BOOTXX?=	/usr/mdec/bootxx

RDSETROOT=	elfrdsetroot

.include "${TOP}/inst-common/Makefile.inc"

all: ${CBIN} ${FLOPPY}

DISKTYPE=       rdroot
NBLKS=          5744

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS=	-t ffs -m 0 -o space
NEWFSARGS_RD=	-i 8192
NEWFSARGS_FD=	-i 524288 -c ${FLOPPYBLKS}

${FLOPPY}:	bsd.gz
	dd if=/dev/zero of=${REALIMAGE} count=${FLOPPYBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${FLOPPYTYPE}
	newfs ${NEWFSARGS} ${NEWFSARGS_FD} -s ${FLOPPYBLKS} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	dd if=${DESTDIR}/${BOOT} of=${MOUNT_POINT}/boot bs=512
	dd if=bsd.gz of=${MOUNT_POINT}/bsd bs=512
	${INSTALLBOOT} ${MOUNT_POINT}/boot ${BOOTXX} ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	@cmp ${DESTDIR}/${BOOT} ${MOUNT_POINT}/boot || echo "Warning! /boot corrupted!!!"
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
	objcopy -Sg -R .comment bsd.rd bsd.strip
	${STRIP} bsd.strip
	${GZIPCMD} -c ${GZIPFLAGS} bsd.strip > bsd.gz

bsd.rd: bsd ${IMAGE} ${RDSETROOT}
	cp bsd bsd.rd
	${.OBJDIR}/${RDSETROOT} bsd.rd ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/conf && config ${RAMDISK}
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK} && \
	    ${MAKE} clean && COPTS=-Os exec ${MAKE}
	cp ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK}/bsd bsd

${IMAGE}:	do_prep do_mount do_files do_umount
	mv -f ${REALIMAGE} ${IMAGE}

elfrdsetroot: ${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o elfrdsetroot ${TOP}/../common/elfrdsetroot.c

do_prep:
	dd if=/dev/zero of=${REALIMAGE} count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	
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

.ifdef RELEASEDIR
beforeinstall:
	cp ${FLOPPY} ${RELEASEDIR}
.endif

