#	$OpenBSD: Makefile,v 1.1 2011/03/14 16:40:23 drahn Exp $

REV=	${OSrev}

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

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$

OBJARG=
# XXX - the following for cross builds. shouldn't be here for native
.if ${HOST} != ${TARGET}
OBJARG=-O obj.${HOST}.${TARGET}
.endif

DISKTYPE=       rdroot2.5M
#NBLKS=		8192
NBLKS=		5120
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -i 4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${BSD_RD}

${BSD_RD}: ${CBIN} ${IMAGE} bsd rdsetroot
	cp bsd ${BSD_RD}
	awk 'BEGIN { printf("%c", 192); }' < /dev/null | dd of=${BSD_RD} bs=1 count=1 seek=95 conv=notrunc 2>/dev/null;
	awk 'BEGIN { printf("%c", 192); }' < /dev/null | dd of=${BSD_RD} bs=1 count=1 seek=127 conv=notrunc 2>/dev/null;
	awk 'BEGIN { printf("%c", 192); }' < /dev/null | dd of=${BSD_RD} bs=1 count=1 seek=159 conv=notrunc 2>/dev/null;
	${.OBJDIR}/rdsetroot ${BSD_RD} ${IMAGE}
	dd if=/dev/zero of=${BSD_RD} bs=1 count=1 seek=95 conv=notrunc 2>/dev/null;
	dd if=/dev/zero of=${BSD_RD} bs=1 count=1 seek=127 conv=notrunc 2>/dev/null;
	dd if=/dev/zero of=${BSD_RD} bs=1 count=1 seek=159 conv=notrunc 2>/dev/null;

${IMAGE}: rd_setup do_files rd_teardown

.endif

bsd:
	cd ${TOP}/../../sys/arch/beagle/conf && config RAMDISK
	cd ${TOP}/../../sys/arch/beagle/compile/RAMDISK && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}
	cp ${TOP}/../../sys/arch/beagle/compile/RAMDISK/bsd bsd

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

rdsetroot:	${TOP}/../common/elfrdsetroot.c
	${HOSTCC} -o rdsetroot ${TOP}/../common/elfrdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

.PRECIOUS:	${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E ${OBJARG} -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

do_files:
	cat ${MTREE} | mtree -de -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    *.o *.lo *.c bsd ${BSD_RD} rdsetroot
	rm -f instbin.conf
	rm -rf cd-dir

.ifdef RELEASEDIR
install:
	cp ${BSD_RD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
