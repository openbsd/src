#	$OpenBSD: Makefile.inc,v 1.5 1997/09/26 02:12:09 millert Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		instbin
COMMONDIR=	${TOP}/inst-common

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE?=		miniroot-${REV}.fs
BOOTSYS=	bootroot-${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN} bootroot
	dd if=/dev/zero of=${IMAGE} bs=64k count=32
	vnconfig -v -c ${VND} ${IMAGE}
	disklabel -w ${VND} miniroot
	newfs -O -m 0 -o space -i 8192 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount -f ${MOUNT_POINT}
	vnconfig -u ${VND}

bootroot: edlabel copy xxboot boot
	dd if=/dev/zero of=${BOOTSYS} bs=64k count=4
	vnconfig -v -c ${VND} ${BOOTSYS}
	disklabel -w ${VND} bootroot
	newfs -O -m 0 -o space -i 8192 ${VND_RDEV}
	dd if=xxboot of=${VND_RDEV} count=16
	mount ${VND_DEV} ${MOUNT_POINT}
	cp edlabel copy boot ${MOUNT_POINT}
	umount -f ${MOUNT_POINT}
	vnconfig -u ${VND}

edlabel copy xxboot boot:
	(cd /sys/arch/vax/boot;make)
	cp /sys/arch/vax/boot/edlabel .
	cp /sys/arch/vax/boot/copy .
	cp /sys/arch/vax/boot/xxboot .
	cp /sys/arch/vax/boot/boot .

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND} ${IMAGE}
	-/bin/rm -f ${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
