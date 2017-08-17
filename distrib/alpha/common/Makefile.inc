#	$OpenBSD: Makefile.inc,v 1.32 2017/08/17 05:24:09 jasper Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs

MOUNT_POINT=	/mnt

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}

STRIP?=		strip
STRIPOPTS?=	-R .SUNW_ctf
GZIPCMD?=	gzip
GZIPFLAGS?=	-9fn
INSTALLBOOT?=	/usr/mdec/installboot -v

XNAME?=		floppy
FLOPPY?=	${XNAME}${REV}.fs
FLOPPYBLKS?=	2880
FLOPPYTYPE?=	floppy
BOOT?=		/usr/mdec/boot
BOOTXX?=	/usr/mdec/bootxx

.include "${TOP}/inst-common/Makefile.inc"

all: ${FLOPPY}

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=8192

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS=	-t ffs -m 0 -o space
NEWFSARGS_FD=	-i 524288 -c ${FLOPPYBLKS}

CLEANFILES+=	bsd bsd.rd bsd.gz ${FLOPPY}

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
	${STRIP} ${STRIPOPTS} bsd.strip
	${GZIPCMD} -c ${GZIPFLAGS} bsd.strip > bsd.gz

bsd.rd: bsd ${IMAGE} rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd ${IMAGE}
	strip -R .SUNW_ctf bsd.rd

bsd:
	cd ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && env COPTS=-Oz ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/alpha/compile/${RAMDISK}/obj/bsd bsd

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

rdsetroot: ${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

unconfig:
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}

.ifdef RELEASEDIR
beforeinstall:
	cp ${FLOPPY} ${RELEASEDIR}
.endif
