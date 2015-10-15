#	$OpenBSD: Makefile,v 1.16 2015/10/15 19:28:32 miod Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/game${OSrev}.tgz ${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.${MACHINE}
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xfont${OSrev}.tgz \
		${RELXDIR}/xshare${OSrev}.tgz ${RELXDIR}/xserv${OSrev}.tgz

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	isoroot
PID!=		echo $$$$
ISOIMAGE!=	echo image.${PID}
ISOSIZE=	798600	# 33*11*2200
NEWFSOPTS=	-t ffs -m 0 -o space -f 1024 -b 8192 -i 8192 -s ${ISOSIZE}

all: ${CDROM}

${CDROM}: ${BASE} ${XBASE}
	-rm -f ${.OBJDIR}/image.*
	dd if=/dev/zero of=${ISOIMAGE} count=${ISOSIZE}
	vnconfig -v -c ${VND} ${ISOIMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	newfs ${NEWFSOPTS} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	installboot -vr ${MOUNT_POINT} ${VND} \
	    ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/usr/mdec/boot
	cp ${RELDIR}/bsd.rd ${MOUNT_POINT}/bsd
	mkdir -p ${MOUNT_POINT}/${OSREV}/${MACHINE}

	cp -p ${BASE} ${MOUNT_POINT}/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${MOUNT_POINT}/${OSREV}/${MACHINE}

	cat ${RELDIR}/SHA256 ${RELXDIR}/SHA256 > \
	    ${MOUNT_POINT}/${OSREV}/${MACHINE}/SHA256
	# XXX no SHA256.sig

	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	mv ${ISOIMAGE} ${CDROM}

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM} ${.OBJDIR}/image.*

unconfig:
	-umount /mnt
	-vnconfig -u vnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
