#	$OpenBSD: Makefile,v 1.11 2015/07/17 17:02:52 jsg Exp $

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

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$

KERNADDR_OMAP=0x80300000
KERNADDR_IMX=0x10300000
KERNADDR_SUNXI=0x40300000
KERNADDR_VEXPRESSA15=0x80300000
KERNADDR_VEXPRESSA9=0x60300000
KERNADDR_EXYNOS=0x40300000

DISKTYPE=       rdroot
NBLKS=		10240
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -i 4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

ALLSOC=		EXYNOS IMX OMAP SUNXI VEXPRESSA9 VEXPRESSA15
ALLBSD=		bsd
ALLBSDRD=	${BSD_RD}
.for SOC in ${ALLSOC}
ALLBSDRD+=	${BSD_RD}.${SOC}.umg
.endfor
all:	${ALLBSDRD}

bsd:
	cd ${TOP}/../../sys/arch/armv7/conf && config RAMDISK
	cd ${TOP}/../../sys/arch/armv7/compile/RAMDISK && \
	    ${MAKE} clean && exec ${MAKE}
	cp ${TOP}/../../sys/arch/armv7/compile/RAMDISK/bsd bsd

${BSD_RD}: ${CBIN} ${IMAGE} bsd rdsetroot
	cp bsd ${BSD_RD}
	${.OBJDIR}/rdsetroot ${BSD_RD} ${IMAGE}

.for SOC in ${ALLSOC}
${BSD_RD}.${SOC}.umg: ${BSD_RD}
	mkuboot -a arm -o linux -e ${KERNADDR_${SOC}} -l ${KERNADDR_${SOC}} \
	    ${BSD_RD} ${BSD_RD}.${SOC}.umg
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
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

do_files:
	cat ${MTREE} | mtree -de -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    *.o *.lo *.c ${ALLBSD} ${ALLBSDRD} rdsetroot
	rm -f instbin.conf
	rm -rf cd-dir

.ifdef RELEASEDIR
install:
	cp ${ALLBSDRD} ${RELEASEDIR}
.endif	# RELEASEDIR

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
