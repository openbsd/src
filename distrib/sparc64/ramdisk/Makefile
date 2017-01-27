#	$OpenBSD: Makefile,v 1.32 2017/01/27 17:59:09 natano Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
BSD_RD=		bsd.rd
IMAGE=		mr.fs
CBIN?=		instbin
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

FLOPPY=		floppy${REV}.fs
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a

GZIPCMD?=	gzip
GZIPFLAGS?=	-9nv
GZIPEXT?=	.gz

PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
KERNEL?=	RAMDISKU1

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${FLOPPY} ${BSD_RD}

bsd.gz:	bsd.rd
	${GZIPCMD} ${GZIPFLAGS} < bsd.rd > bsd.gz

${FLOPPY}:	bsd.gz ${BOOT} /usr/
	dd if=/dev/zero of=${REALIMAGE} count=${FLOPPYSIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${FLOPPYTYPE}
	newfs -m 0 -o space -i 524288 -c ${FLOPPYSIZE} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${DESTDIR}/usr/mdec/ofwbootfd ${MOUNT_POINT}/ofwboot
	cp bsd.gz ${MOUNT_POINT}/bsd
	installboot -v ${VND} /usr/mdec/bootblk ${MOUNT_POINT}/ofwboot
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	dd if=${REALIMAGE} of=${FLOPPY} count=${FLOPPYSIZE}
	rm -f ${REALIMAGE}

${BSD_RD}: ${CBIN} ${IMAGE} bsd elfrdsetroot
	cp bsd ${BSD_RD}
	${.OBJDIR}/elfrdsetroot ${BSD_RD} < ${IMAGE}

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

bsd:
	cd ${TOP}/../../sys/arch/sparc64/compile/${KERNEL} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && env COPTS=-Os ${MAKE}'
	cp -p ${TOP}/../../sys/arch/sparc64/compile/${KERNEL}/obj/bsd bsd

elfrdsetroot:	${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o elfrdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    *.o *.lo *.c bsd ${BSD_RD} bsd.gz ${FLOPPY} elfrdsetroot
	/bin/rm -rf ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${FLOPPY} ${BSD.RD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
