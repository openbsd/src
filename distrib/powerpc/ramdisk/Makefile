#	$OpenBSD: Makefile,v 1.3 1998/05/29 04:34:19 rahnds Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
BSD_RD=		bsd.rd
BSDOFW_RD=	bsdofw.rd
IMAGE=		mr.fs
CBIN?=		instbin
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${.CURDIR}/mtree.conf

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$


DISKTYPE=       rdroot
NBLKS=          8192
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -c 16 -i 4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before makeing a ramdisk!
	@false
.else

all:	${BSD_RD} ${BSDOFW_RD}

${BSD_RD}: ${CBIN} ${IMAGE} bsd rdsetroot
	cp bsd ${BSD_RD}
	${.OBJDIR}/rdsetroot ${BSD_RD} < ${IMAGE}

${BSDOFW_RD}: ${CBIN} ${IMAGE} bsdofw rdsetroot
	cp bsdofw ${BSDOFW_RD}
	${.OBJDIR}/rdsetroot ${BSDOFW_RD} < ${IMAGE}

${IMAGE}: rd_setup do_files rd_teardown

.endif

bsdofw:
	cd ${.TOP}/../../sys/arch/powerpc/conf && config RAMDISKOFW
	cd ${.TOP}/../../sys/arch/powerpc/compile/RAMDISKOFW && \
	    make clean && make
	cp ${.TOP}/../../sys/arch/powerpc/compile/RAMDISKOFW/bsd bsdofw

bsd:
	cd ${.TOP}/../../sys/arch/powerpc/conf && config RAMDISK
	cd ${.TOP}/../../sys/arch/powerpc/compile/RAMDISK && \
	    make clean && make
	cp ${.TOP}/../../sys/arch/powerpc/compile/RAMDISK/bsd bsd

rd_setup: 
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${IMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}

rdsetroot:	${TOP}/common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/common/rdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

.PRECIOUS:	${IMAGE}

install:
	cp ${BSD_RD} ${DESTDIR}/snapshot/${BSD_RD}
	cp ${BSDOFW_RD} ${DESTDIR}/snapshot/${BSDOFW_RD}

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${CBIN}.conf

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.conf ${CBIN}.mk ${CBIN}.cache \
	*.o *.lo *.c bsd ${BSD_RD} ${BSD_RD}.elf rdsetroot

beforeinstall:
	cp ${BSD.RD} ${DESTDIR}/snapshot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
