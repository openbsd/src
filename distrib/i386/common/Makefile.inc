#	$OpenBSD: Makefile.inc,v 1.40 2016/11/24 16:32:33 natano Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CBIN?=		instbin
CRUNCHCONF?=	${CBIN}.conf
LISTS?=		${.CURDIR}/../common/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

XNAME?=		floppy
FS?=		${XNAME}${REV}.fs
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
BOOT?=		${DESTDIR}/usr/mdec/boot
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy3

all:	${FS}

${FS}:	bsd.gz
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${FLOPPYSIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
.ifdef LBA
	fdisk -yi -l ${FLOPPYSIZE} -f ${DESTDIR}/usr/mdec/mbr ${VND}
.endif
	disklabel -w ${VND} ${FLOPPYTYPE}
	newfs -m 0 -o space -i 524288 -c ${FLOPPYSIZE} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${BOOT} ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	strip -R .comment ${.OBJDIR}/boot
	installboot -v -r ${MOUNT_POINT} ${VND_CRDEV} \
	    ${DESTDIR}/usr/mdec/biosboot ${.OBJDIR}/boot
	dd if=bsd.gz of=${MOUNT_POINT}/bsd bs=512
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${FS}
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}


DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

COPTS= -Os -mtune=i486

bsd.gz: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	strip -R .comment bsd.strip
	gzip -c9n bsd.strip > bsd.gz

bsd.rd:	${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd ${IMAGE}


bsd:
	cd ${.CURDIR}/../../../sys/arch/i386/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && env COPTS="${COPTS}" ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/i386/compile/${RAMDISK}/obj/bsd bsd

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

rdsetroot:	${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

.ifdef RELEASEDIR
install:
.ifndef NOBSDRD
	cp bsd.rd ${RELEASEDIR}/bsd.rd
.endif
.ifndef NOFS
	cp ${FS} ${RELEASEDIR}/${FS}
.endif
.endif	# RELEASEDIR

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c ${CBIN}.c -e ${CBIN} -m ${CBIN}.mk ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	strip -R .comment ${CBIN}

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CRUNCHCONF}

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}*.cache \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs \
	    lib*.a lib*.olist ${CBIN}.map \
	    rdsetroot boot ${CRUNCHCONF} ${FS}
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
