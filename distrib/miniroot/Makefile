#	$OpenBSD: Makefile,v 1.21 1998/03/08 00:26:22 deraadt Exp $

REV=		23

CBIN=		instbin
ARCHDIR=	${.CURDIR}/../${MACHINE}/miniroot

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	miniroot
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
IMAGE?=		miniroot${REV}.fs
IMAGESIZE?=	10240 # 5Mb in 512 byte blocks
NEWFSOPTS?=
NEWFS_WILL_FAIL?=false
CRUNCHGENOPTS?=

LISTS=		${.CURDIR}/list ${ARCHDIR}/list
CRUNCHCONF=	${CBIN}.conf
MTREE=		${.CURDIR}/mtree.conf
RAWLABEL?=	-r

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before makeing a miniroot!
	@false
.else

bsd:
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/conf && config GENERIC
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/compile/GENERIC && \
	    make clean && make
	cp ${.CURDIR}/../../sys/arch/${MACHINE}/compile/GENERIC/bsd bsd

all: ${CBIN} bsd
	dd if=/dev/zero of=${REALIMAGE} count=${IMAGESIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${RAWLABEL} ${VND} ${DISKTYPE}
	newfs -m 0 -o space ${NEWFSOPTS} ${VND_RDEV} || ${NEWFS_WILL_FAIL}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	REV=${REV} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} ARCHDIR=${ARCHDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${.CURDIR}/runlist.sh ${LISTS}
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
	make -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	${CBIN}.conf *.o *.lo *.c

beforeinstall:
	cp ${IMAGE} ${DESTDIR}/snapshot

.endif	# DESTDIR check

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
