#	$OpenBSD: Makefile,v 1.17 1997/09/26 02:09:55 millert Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		kc${REV}.fs
CBIN=		kcbin
MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE?=		xxx-${REV}.fs
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
MDEC=		${DESTDIR}/usr/mdec

LIST=		${.CURDIR}/list
CRUNCHCONF=	${.CURDIR}/${CBIN}.conf
MTREE=		${.CURDIR}/mtree.conf

bsd:
	cd ${.CURDIR}/../../../../sys/arch/i386/conf && config INST
	cd ${.CURDIR}/../../../../sys/arch/i386/compile/INST && \
	    make clean && make
	cp ${.CURDIR}/../../../../sys/arch/i386/compile/INST/bsd bsd
	strip bsd

all: bsd ${CBIN}
.ifndef FLOPPY3
.else
.endif
.ifndef FLOPPY3
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=120
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -B -b /usr/mdec/fdboot -s /usr/mdec/bootfd ${VND} floppy5
	newfs -O -m 0 -o space -i 8192 -c 80 ${VND_RDEV}
.else
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -B -b /usr/mdec/fdboot -s /usr/mdec/bootfd ${VND} floppy3
	newfs -O -m 0 -o space -i 6500 -c 80 ${VND_RDEV}
.endif
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LIST}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${TOP}/../../.. -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

clean cleandir:
	/bin/rm -f *.core bsd ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

install:
	cp ${IMAGE} ${DESTDIR}/snapshot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
