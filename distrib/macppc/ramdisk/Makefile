#	$OpenBSD: Makefile,v 1.23 2009/04/17 03:58:54 deraadt Exp $

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


DISKTYPE=       rdroot
NBLKS=          8192
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -i 4096

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
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/macppc/
	cp bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	strip ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	gzip -9n ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	mv ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd.gz ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd
	# On HFS, doing the following means COPIES of the files.. therefore
	# we skip doing so.
	# ln ${.OBJDIR}/cd-dir/${OSREV}/macppc/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	# ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	cp ${.CURDIR}/../../../sys/arch/macppc/stand/tbxidata/bsd.tbxi \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp ${DESTDIR}/usr/mdec/ofwboot ${.OBJDIR}/cd-dir/
	mkhybrid -v -v -r -part -hfs \
	    -hfs-bless ${.OBJDIR}/cd-dir/${OSREV}/macppc \
	    -map ${.CURDIR}/../../../gnu/usr.sbin/mkhybrid/src/more.mapping \
	    -A "OpenBSD ${OSREV} macppc bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/macppc  ${OSREV} boot-only CD" \
	    -o ${.OBJDIR}/cd${OSrev}.iso ${.OBJDIR}/cd-dir

${IMAGE}: rd_setup do_files rd_teardown

.endif

bsd:
	cd ${TOP}/../../sys/arch/macppc/conf && config RAMDISK
	cd ${TOP}/../../sys/arch/macppc/compile/RAMDISK && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}
	cp ${TOP}/../../sys/arch/macppc/compile/RAMDISK/bsd bsd

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
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

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
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	*.o *.lo *.c bsd ${BSD_RD} cd${REV}.iso \
	rdsetroot
	rm -rf cd-dir

.ifdef RELEASEDIR
install:
	cp ${BSD_RD} cd${REV}.iso ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
