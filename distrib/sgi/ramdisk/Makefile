#	$OpenBSD: Makefile,v 1.21 2015/09/20 12:55:11 miod Exp $

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

ALLIP=		22 26 27 28 30 32
ALLBSD=
ALLBSDRD=
.for IP in ${ALLIP}
ALLBSD+=	bsd.IP${IP}
ALLBSDRD+=	${BSD_RD}.IP${IP}
all:	${ALLBSDRD}
.endfor

.for IP in ${ALLIP}
bsd.IP${IP}:
.ifndef NOBUILD
	cd ${.TOP}/../../sys/arch/sgi/conf && config RAMDISK-IP${IP}
	cd ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP${IP} && \
	    ${MAKE} clean && exec ${MAKE}
.endif
	cp ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP${IP}/bsd bsd.IP${IP}

${BSD_RD}.IP${IP}: ${CBIN} ${IMAGE} bsd.IP${IP} rdsetroot
	cp bsd.IP${IP} ${BSD_RD}.IP${IP}
	${.OBJDIR}/rdsetroot ${BSD_RD}.IP${IP} ${IMAGE}
.endfor

${IMAGE}: rd_setup do_files rd_teardown

.endif

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
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

.PRECIOUS:	${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -M -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

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
	    lib*.a lib*.olist ${CBIN}.map \
	    ${ALLBSD} ${ALLBSDRD} *.o *.lo *.c rdsetroot

.ifdef RELEASEDIR
install:
	cp ${ALLBSDRD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
