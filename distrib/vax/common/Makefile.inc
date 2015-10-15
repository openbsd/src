#	$OpenBSD: Makefile.inc,v 1.11 2015/10/15 19:28:32 miod Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		instbin
COMMONDIR=	${TOP}/common
UTILS=		${TOP}/../miniroot

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE?=		miniroot-${REV}.fs
BOOTSYS=	bootroot-${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
CRUNCHCONF=	${CBIN}.conf
MTREE=		${UTILS}/mtree.conf

all: ${CBIN} bootroot
	dd if=/dev/zero of=${IMAGE} bs=64k count=32
	vnconfig -v -c ${VND} ${IMAGE}
	disklabel -w ${VND} miniroot
	newfs -O 0 -m 0 -o space -i 8192 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount -f ${MOUNT_POINT}
	vnconfig -u ${VND}

bootroot:
	dd if=/dev/zero of=${BOOTSYS} bs=512 count=8192
	vnconfig -v -c ${VND} ${BOOTSYS}
	disklabel -w ${VND} bootroot
	newfs -O 0 -m 0 -o space -i 8192 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	installboot -vr ${MOUNT_POINT} ${VND} \
	    ${DESTDIR}/usr/mdec/xxboot ${DESTDIR}/usr/mdec/boot
	umount -f ${MOUNT_POINT}
	vnconfig -u ${VND}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND} ${IMAGE}
	-/bin/rm -f ${IMAGE}

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${TOP}/../../ -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

#${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
#	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist ${CBIN}.map

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
