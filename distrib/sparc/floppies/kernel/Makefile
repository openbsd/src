#	$Id: Makefile,v 1.4 1996/10/18 23:35:36 deraadt Exp $

.include "../Makefile.inc"

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE=		kc${REV}.fs
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${.CURDIR}/list

bsd:
	cd ${.CURDIR}/../../../../sys/arch/sparc/conf && config FLOPPY
	cd ${.CURDIR}/../../../../sys/arch/sparc/compile/FLOPPY && \
	    make clean && make depend && make
	cp ${.CURDIR}/../../../../sys/arch/sparc/compile/FLOPPY/bsd bsd

all:	bsd
.if ${VND} == vnd0
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND_DEV} ${REALIMAGE}
	# fuck, disklabeling completely fails
.else
	disklabel -w ${VND_RDEV} floppy
.endif
	newfs -O -m 0 -o space -i 8192 -c 80 ${VND_RDEV} floppy
	mount ${VND_DEV} ${MOUNT_POINT}
	sync; /usr/mdec/binstall -v ffs ${MOUNT_POINT}
	TOPDIR=${.CURDIR}/.. CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${.CURDIR}/../runlist.sh ${LISTS}
	mtree -def ${.CURDIR}/mtree.conf -p ${MOUNT_POINT}/ -u
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
.if ${VND} == vnd0
	vnconfig -u ${VND_DEV}
	cat /bin/* > /dev/null		# flush buffer cache (yuck)
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}
	echo you have just produced a perfectly wonderful disfunctional floppy
.else
	dd if=${VND_RDEV} of=${IMAGE} bs=10k
.endif


unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
	-/bin/rm -f ${IMAGE}

clean cleandir:
	/bin/rm -f ${IMAGE} bsd

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
