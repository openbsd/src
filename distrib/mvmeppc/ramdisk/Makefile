#	$OpenBSD: Makefile,v 1.14 2009/04/17 03:58:54 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
BSD_RD=		bsd.rd
IMAGE=		mr.fs
CBIN?=		instbin
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$


DISKTYPE=       rdroot
NBLKS=          8192
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${BSD_RD} cd

${BSD_RD}: ${CBIN} ${IMAGE} bsd rdsetroot
	cp bsd ${BSD_RD}
	${.OBJDIR}/rdsetroot ${BSD_RD} ${IMAGE}

cd: ${BSD_RD}
	-rm -rf ${.OBJDIR}/cd-dir/
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/mvmeppc/
	cp bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/mvmeppc/bsd.rd
	strip ${.OBJDIR}/cd-dir/${OSREV}/mvmeppc/bsd.rd
	gzip -9n ${.OBJDIR}/cd-dir/${OSREV}/mvmeppc/bsd.rd
	mv ${.OBJDIR}/cd-dir/${OSREV}/mvmeppc/bsd.rd.gz ${.OBJDIR}/cd-dir/${OSREV}/mvmeppc/bsd.rd


${IMAGE}: rd_setup do_files rd_teardown

.endif

bsd:
	cd ${TOP}/../../sys/arch/mvmeppc/conf && config RAMDISK
	cd ${TOP}/../../sys/arch/mvmeppc/compile/RAMDISK && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}
	cp ${TOP}/../../sys/arch/mvmeppc/compile/RAMDISK/bsd bsd

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

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	*.o *.lo *.c bsd ${BSD_RD} \
	rdsetroot

.ifdef RELEASEDIR
install:
	cp ${BSD.RD} cd${REV}.fs ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
