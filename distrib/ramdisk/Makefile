#	$OpenBSD: Makefile,v 1.1 1997/05/14 20:44:34 deraadt Exp $
#	$OpenBSD: Makefile,v 1.1 1997/05/14 20:44:34 deraadt Exp $
#	$NetBSD: Makefile,v 1.2.4.3 1996/07/04 07:05:32 leo Exp $

# Revision is 2.1
REV=		21

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
IMAGESIZE?=	4096
NEWFSOPTS?=	-m 0 -c 64 -i 8192
CRUNCHGENOPTS?=
KERNEL?=	RAMDISK
UTILS?=		${.CURDIR}/../miniroot
STRIP?=		strip

LISTS=		${.CURDIR}/list ${ARCHDIR}/list
MTREE=		${.CURDIR}/mtree.conf

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before makeing a miniroot!
	@false
.else

all:	bsd.rd

bsd:
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/conf && config ${KERNEL}
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${KERNEL} && \
	    make clean && make depend && make
	cp ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${KERNEL}/bsd bsd


bsd.rd: bsd ${IMAGE} ${.OBJDIR}/rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}
	${STRIP} bsd.rd

${.OBJDIR}/rdsetroot: ${.CURDIR}/../common/rdsetroot.c
	${HOSTCC} ${HOSTCCFLAGS} -o rdsetroot ${.CURDIR}/../common/rdsetroot.c


${IMAGE}: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} count=${IMAGESIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	newfs -o space ${NEWFSOPTS} ${VND_RDEV} ${DISKTYPE}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	REV=${REV} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} ARCHDIR=${ARCHDIR} \
	    TARGDIR=${MOUNT_POINT} UTILS=${UTILS} sh ${UTILS}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}
	cat /*bin/* > /dev/null
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

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

.endif	# DESTDIR check
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
