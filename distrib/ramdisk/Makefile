#	$OpenBSD: Makefile,v 1.14 1998/06/27 20:52:11 todd Exp $

REV=		${OSrev}

CBIN=		instbin
ARCHDIR=	${.CURDIR}/../${MACHINE}/ramdisk

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	rdroot
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}
IMAGE?=		ramdisk${REV}.fs
IMAGESIZE?=	8192 # 4Mb in 512 byte blocks
NEWFS_WILL_FAIL?=false
CRUNCHGENOPTS?=
KERNEL?=	RAMDISK
UTILS?=		${.CURDIR}/../miniroot
STRIP?=		strip

LISTS=		${.CURDIR}/list ${ARCHDIR}/list
MTREE=		${.CURDIR}/mtree.conf

FLOPPY?=	floppy${REV}.fs
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy
INSTALLBOOT?=	installboot
BOOT?=		${DESTDIR}/usr/mdec/boot
BOOTXX?=	${DESTDIR}/usr/mdec/bootxx
RAWLABEL?=	-r

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSOPTS=	-t ffs -m 0 -o space -f 1024 -b 8192
NEWFSOPTS_RD=	${NEWFSOPTS} -i 16384 -c 64 -s ${IMAGESIZE}
#NEWFSOPTS_FD=	${NEWFSOPTS} -i 524288 -c 80 -s ${FLOPPYSIZE}
NEWFSOPTS_FD=	${NEWFSOPTS} -i 1048575 -c 256 -s ${FLOPPYSIZE}

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before makeing a ramdisk!
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
	disklabel -w ${RAWLABEL} ${VND} ${FLOPPYTYPE}
	newfs ${NEWFSOPTS_FD} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${BOOT} ${MOUNT_POINT}/boot
	cp bsd.gz ${MOUNT_POINT}/bsd
	${INSTALLBOOT} -v ${MOUNT_POINT}/boot ${BOOTXX} \
		`echo ${VND_RDEV} | sed -e 's/a$$/c/'`
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
	    make depend && make clean && make
	cp ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${KERNEL}/bsd bsd

bsd.rd:	bsd ${IMAGE} rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}
	${STRIP} bsd.rd

bsd.gz:	bsd.rd
	${GZIP} ${GZIPFLAGS} < bsd.rd > bsd.gz

rdsetroot: ${.CURDIR}/../common/rdsetroot.c
	${HOSTCC} ${HOSTCCFLAGS} -o rdsetroot ${.CURDIR}/../common/rdsetroot.c

${IMAGE}: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} count=${IMAGESIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${RAWLABEL} ${VND} ${DISKTYPE}
	newfs ${NEWFSOPTS_RD} ${VND_RDEV} || ${NEWFS_WILL_FAIL}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	REV=${REV} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} ARCHDIR=${ARCHDIR} \
	    TARGDIR=${MOUNT_POINT} UTILS=${UTILS} sh ${UTILS}/runlist.sh ${LISTS}
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
	make -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	${CBIN}.conf *.o *.lo *.c bsd bsd.*

beforeinstall:
	cp bsd.rd ${DESTDIR}/snapshot
.ifdef MAKEFLOPPY
	cp ${FLOPPY} ${DESTDIR}/snapshot
.endif

.endif	# DESTDIR check

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${REALIMAGE}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
