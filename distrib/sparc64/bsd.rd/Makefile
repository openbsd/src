#	$OpenBSD: Makefile,v 1.1 2001/10/10 04:21:02 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		ramdisk${REV}.fs
CBIN?=		instbin
CRUNCHCONF?=    ${TOP}/common/${CBIN}.conf
CRUNCHCONF?=    ${.CURDIR}/../common/${CBIN}.conf
LISTS?=		${.CURDIR}/../common/list ${.CURDIR}/list

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
MTREE=		${.CURDIR}/../common/mtree.conf

RAMDISK=	RAMDISK
BOOT?=		/usr/mdec/boot
BOOTXX?=	/usr/mdec/bootxx

RDSETROOT=	elfrdsetroot

#.include "${TOP}/common/Makefile.inc"

all: ${CBIN} bsd.rd
	@echo "all done"

DISKTYPE=       rdroot
NBLKS=          5744

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS=	-t ffs -m 0 -o space
NEWFSARGS_RD=	-i 8192 -c 80
NEWFSARGS_FD=	-i 524288 -c 80

bsd.rd:	bsd.rd_unz
	objcopy -Sg -R .comment bsd.rd_unz bsd.strip
	${STRIP} bsd.strip
	${GZIP} -c ${GZIPFLAGS} bsd.strip > bsd.rd

bsd.rd_unz: bsd ${IMAGE} ${RDSETROOT}
	cp bsd bsd.rd_unz
	${.OBJDIR}/${RDSETROOT} bsd.rd_unz < ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../sys/arch/sparc64/conf && config ${RAMDISK}
	cd ${.CURDIR}/../../../sys/arch/sparc64/compile/${RAMDISK} && \
	    make depend && make clean && COPTS=-Os make
	cp ${.CURDIR}/../../../sys/arch/sparc64/compile/${RAMDISK}/bsd bsd

${IMAGE}:	do_prep do_mount do_files do_umount
	mv -f ${REALIMAGE} ${IMAGE}

ecoffrdsetroot:	${TOP}/ecoffrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o ecoffrdsetroot ${TOP}/ecoffrdsetroot.c

elfrdsetroot: ${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o elfrdsetroot ${TOP}/../common/elfrdsetroot.c

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

${IMAGE}: ${CBIN} rd_setup do_files rd_teardown

rd_setup: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

rdsetroot:	${TOP}/../../common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/../../common/rdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.PRECIOUS:	${IMAGE}

install:
.ifndef NOBSDRD
	cp bsd.rd ${DESTDIR}/snapshot/bsd.rd
.endif

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c ${CBIN}.c -e ${CBIN} -m ${CBIN}.mk ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} sh ${TOP}/common/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}*.cache \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs rdsetroot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
