#	$OpenBSD: Makefile,v 1.1 2001/09/01 16:47:03 drahn Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
BSD_RD=		bsd.rd
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
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${BSD_RD} cd

${BSD_RD}: ${CBIN} ${IMAGE} bsd rdsetroot
	cp bsd ${BSD_RD}
	${.OBJDIR}/rdsetroot ${BSD_RD} < ${IMAGE}

cd: ${BSD_RD}
	-rm -rf ${.OBJDIR}/cd-dir/
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/powerpc/
	cp bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/powerpc/bsd.rd
	strip ${.OBJDIR}/cd-dir/${OSREV}/powerpc/bsd.rd
	gzip -9 ${.OBJDIR}/cd-dir/${OSREV}/powerpc/bsd.rd
	mv ${.OBJDIR}/cd-dir/${OSREV}/powerpc/bsd.rd.gz ${.OBJDIR}/cd-dir/${OSREV}/powerpc/bsd.rd
	cp ${DESTDIR}/usr/mdec/ofwboot ${.OBJDIR}/cd-dir/
	mkhybrid -r -part -hfs -o ${.OBJDIR}/cd${OSrev}.fs ${.OBJDIR}/cd-dir


${IMAGE}: rd_setup do_files rd_teardown

.endif

bsd:
	cd ${TOP}/../../sys/arch/powerpc/conf && config RAMDISK
	cd ${TOP}/../../sys/arch/powerpc/compile/RAMDISK && \
	    make clean && make depend && make
	cp ${TOP}/../../sys/arch/powerpc/compile/RAMDISK/bsd bsd

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
	cp ${BSD_RD} cd${REV}.fs ${DESTDIR}/snapshot/

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	*.o *.lo *.c bsd ${BSD_RD} \
	rdsetroot

beforeinstall:
	cp ${BSD.RD} cd${REV}.fs ${DESTDIR}/snapshot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
