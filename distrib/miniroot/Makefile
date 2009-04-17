#	$OpenBSD: Makefile,v 1.31 2009/04/17 03:58:54 deraadt Exp $

REV=		${OSrev}

CBIN=		instbin
ARCHDIR=	${.CURDIR}/../${MACHINE}/miniroot

.include <bsd.own.mk>

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

UTILS=		${.CURDIR}

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	miniroot
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
IMAGE?=		miniroot${REV}.fs
IMAGESIZE?=	10240 # 5MB in 512 byte blocks
NEWFSOPTS?=
NEWFS_WILL_FAIL?=false
.if ${ELF_TOOLCHAIN:L} == "yes"
CRUNCHGENOPTS?=-E
.else
CRUNCHGENOPTS?=
.endif

LISTS=		${.CURDIR}/list ${ARCHDIR}/list
CRUNCHCONF=	${CBIN}.conf
MTREE=		${.CURDIR}/mtree.conf

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before making a miniroot!
	@false
.else

bsd:
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/conf && config GENERIC
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/compile/GENERIC && \
	    ${MAKE} clean && ${MAKE} depend && ${MAKE}
	cp ${.CURDIR}/../../sys/arch/${MACHINE}/compile/GENERIC/bsd bsd

all: ${CBIN} bsd
	dd if=/dev/zero of=${REALIMAGE} count=${IMAGESIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} ${DISKTYPE}
	newfs -m 0 -o space ${NEWFSOPTS} ${VND_RDEV} || ${NEWFS_WILL_FAIL}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	REV=${REV} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    UTILS=${UTILS} ARCHDIR=${ARCHDIR} TARGDIR=${MOUNT_POINT} \
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
	awk -f ${.CURDIR}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen ${CRUNCHGENOPTS} -D ${.CURDIR}/../.. -L ${DESTDIR}/usr/lib \
	    ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	${CBIN}.conf *.o *.lo *.c

.endif	# DESTDIR check

.ifdef RELEASEDIR
beforeinstall:
	cp ${IMAGE} ${RELEASEDIR}
.endif

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
