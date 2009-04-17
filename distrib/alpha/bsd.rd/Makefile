#	$OpenBSD: Makefile,v 1.15 2009/04/17 03:58:54 deraadt Exp $

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

RAMDISK=	RAMDISKBIG
BOOT?=		/usr/mdec/boot
BOOTXX?=	/usr/mdec/bootxx

RDSETROOT=	elfrdsetroot

.include "${TOP}/inst-common/Makefile.inc"

LISTS+=		${.CURDIR}/list

all: ${CBIN} bsd.rd

DISKTYPE=       rdroot
NBLKS=          5744

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS=	-t ffs -m 0 -o space
NEWFSARGS_RD=	-i 8192
NEWFSARGS_FD=	-i 524288 -c ${FLOPPYBLKS}

bsd.rd:	bsd.rd_unz
	objcopy -Sg -R .comment bsd.rd_unz bsd.strip
	${STRIP} bsd.strip
	${GZIPCMD} -c ${GZIPFLAGS} bsd.strip > bsd.rd

bsd.rd_unz: bsd ${IMAGE} ${RDSETROOT}
	cp bsd bsd.rd_unz
	${.OBJDIR}/${RDSETROOT} bsd.rd_unz ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/conf && config ${RAMDISK}
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK} && \
	    ${MAKE} clean && ${MAKE} depend && COPTS=-Os ${MAKE}
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
	cp bsd.rd ${RELEASEDIR}
.endif
