#	$OpenBSD: Makefile,v 1.15 2011/07/08 21:39:32 drahn Exp $

REV=		${OSrev}

TOP=		${.CURDIR}/..

BSD_RD=		bsd.rd
IMAGE=		mr.fs
CBIN?=		instbin
CRUNCHCONF?=	${CBIN}.conf
CRUNCHCONFS?=	${.CURDIR}/../common/crunch.conf
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$


DISKTYPE=       rdroot
NBLKS=          10240
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -i 4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before makeing a ramdisk!
	@false
.else

all:	${BSD_RD}.IP27 ${BSD_RD}.IP30 ${BSD_RD}.IP32

${BSD_RD}.IP27: ${CBIN} ${IMAGE} bsd.IP27 rdsetroot
	cp bsd.IP27 ${BSD_RD}.IP27
	${.OBJDIR}/rdsetroot ${BSD_RD}.IP27 ${IMAGE}

${BSD_RD}.IP30: ${CBIN} ${IMAGE} bsd.IP30 rdsetroot
	cp bsd.IP30 ${BSD_RD}.IP30
	${.OBJDIR}/rdsetroot ${BSD_RD}.IP30 ${IMAGE}

${BSD_RD}.IP32: ${CBIN} ${IMAGE} bsd.IP32 rdsetroot
	cp bsd.IP32 ${BSD_RD}.IP32
	${.OBJDIR}/rdsetroot ${BSD_RD}.IP32 ${IMAGE}

${IMAGE}: rd_setup do_files rd_teardown

.endif

bsd.IP27:
.ifndef NOBUILD
	cd ${.TOP}/../../sys/arch/sgi/conf && config RAMDISK-IP27
	cd ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP27 && \
	    ${MAKE} clean && exec ${MAKE}
.endif
	cp ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP27/bsd bsd.IP27

bsd.IP30:
.ifndef NOBUILD
	cd ${.TOP}/../../sys/arch/sgi/conf && config RAMDISK-IP30
	cd ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP30 && \
	    ${MAKE} clean && ${MAKE}
.endif
	cp ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP30/bsd bsd.IP30

bsd.IP32:
.ifndef NOBUILD
	cd ${.TOP}/../../sys/arch/sgi/conf && config RAMDISK-IP32
	cd ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP32 && \
	    ${MAKE} clean && ${MAKE}
.endif
	cp ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP32/bsd bsd.IP32

rd_setup: 
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${IMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}

rdsetroot:	${TOP}/../common/rdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o elfrdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

.PRECIOUS:	${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -M -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    bsd.IP27 ${BSD_RD}.IP27 \
	    bsd.IP30 ${BSD_RD}.IP30 \
	    bsd.IP32 ${BSD_RD}.IP32 \
	    *.o *.lo *.c rdsetroot

.ifdef RELEASEDIR
install:
	cp ${BSD_RD}.IP27 ${BSD_RD}.IP30 ${BSD_RD}.IP32 ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
