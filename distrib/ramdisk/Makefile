#	$OpenBSD: Makefile,v 1.39 2011/07/09 00:49:28 drahn Exp $

REV=		${OSrev}

CBIN=		instbin
ARCHDIR=	${.CURDIR}/../${MACHINE}/ramdisk

.include <bsd.own.mk>

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	rdroot
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
IMAGE?=		ramdisk${REV}.fs
IMAGESIZE?=	8192 # 4MB in 512 byte blocks
NEWFS_WILL_FAIL?=false
.if ${ELF_TOOLCHAIN:L} == "yes"
RDSETROOT?=	elfrdsetroot
RDSETROOT_EXT_SRC=${.CURDIR}/../common/elf32.c ${.CURDIR}/../common/elf64.c
CRUNCHGENOPTS?=-E
.else
RDSETROOT?=	rdsetroot
CRUNCHGENOPTS?=
RDSETROOT_EXT_SRC=
.endif
KERNEL?=	RAMDISK
UTILS?=		${.CURDIR}/../miniroot
STRIP?=		strip

LISTS=		${.CURDIR}/list ${ARCHDIR}/list.local
MTREE=		${UTILS}/mtree.conf

FLOPPY?=	floppy${REV}.fs
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy
INSTALLBOOT?=	installboot
BOOT?=		${DESTDIR}/usr/mdec/boot
BOOTIN?=	${MOUNT_POINT}/boot
BOOTXX?=	${DESTDIR}/usr/mdec/bootxx

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSOPTS=	-t ffs -m 0 -o space -f 1024 -b 8192
NEWFSOPTS_RD?=	${NEWFSOPTS} -i 8192 -s ${IMAGESIZE}
#NEWFSOPTS_FD=	${NEWFSOPTS} -i 524288 -c ${FLOPPYSIZE} -s ${FLOPPYSIZE}
NEWFSOPTS_FD=	${NEWFSOPTS} -i 1048575 -c ${FLOPPYSIZE} -s ${FLOPPYSIZE}

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

.ifdef MAKEFLOPPY
all:	bsd.rd ${FLOPPY}
.else
all:	bsd.rd
.endif

${FLOPPY}:	bsd.gz ${BOOT} ${BOOTXX}
	dd if=/dev/zero of=${REALIMAGE} count=${FLOPPYSIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${FLOPPYTYPE}
	newfs ${NEWFSOPTS_FD} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${BOOT} ${MOUNT_POINT}/
	cp bsd.gz ${MOUNT_POINT}/bsd
	${INSTALLBOOT} -v ${BOOTIN} ${BOOTXX} \
		`echo ${VND_RDEV} | sed -e 's/a$$/c/'`
.if (${MACHINE_ARCH} == "vax")
	disklabel -B -b ${BOOTXX} ${VND}
.endif
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	dd if=${REALIMAGE} of=${FLOPPY} count=${FLOPPYSIZE}
	rm -f ${REALIMAGE}

bsd:
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/conf && config ${KERNEL}
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${KERNEL} && \
	    ${MAKE} clean && ${MAKE} depend && COPTS=-Os ${MAKE}
	cp ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${KERNEL}/bsd bsd

bsd.rd:	bsd ${IMAGE} ${RDSETROOT}
	cp bsd bsd.rd
	${.OBJDIR}/${RDSETROOT} bsd.rd ${IMAGE}
	${STRIP} bsd.rd

bsd.gz:	bsd.rd
	${GZIPCMD} ${GZIPFLAGS} < bsd.rd > bsd.gz

${RDSETROOT}: ${.CURDIR}/../common/${RDSETROOT}.c
	${HOSTCC} ${HOSTCCFLAGS} -o ${RDSETROOT} ${RDSETROOT_EXT_SRC} \
	    ${.CURDIR}/../common/${RDSETROOT}.c

${IMAGE}: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} count=${IMAGESIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	newfs ${NEWFSOPTS_RD} ${VND_RDEV} || ${NEWFS_WILL_FAIL}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	REV=${REV} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    ARCHDIR=${ARCHDIR} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen ${CRUNCHGENOPTS} -D ${.CURDIR}/../.. -L ${DESTDIR}/usr/lib \
	    ${CBIN}.conf

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	${CBIN}.conf *.o *.lo *.c bsd bsd.*

.endif	# DESTDIR check

.ifdef RELEASEDIR
beforeinstall:
	cp bsd.rd ${RELEASEDIR}
.ifdef MAKEFLOPPY
	cp ${FLOPPY} ${RELEASEDIR}
.endif	# MAKEFLOPPY
.endif	# RELEASEDIR

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${REALIMAGE}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
