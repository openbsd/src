#	$OpenBSD: Makefile,v 1.13 1998/06/27 20:52:12 todd Exp $

REV=		${OSrev}

ARCHDIR=	${.CURDIR}/../${MACHINE}/simpleroot

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	simpleroot
PID!=		echo $$$$
IMAGEDIR?=	/var/tmp
REALIMAGE!=	echo ${IMAGEDIR}/image.${PID}
IMAGE?=		${IMAGEDIR}/simpleroot${REV}.fs
NEWFSOPTS?=
GZIP?=

LIST=		${ARCHDIR}/list

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before makeing a miniroot!
	@false
.else

all:
	dd if=/dev/zero of=${REALIMAGE} count=${IMAGESIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
.ifdef BOOT_DISKLABEL
	disklabel -B ${BOOT_DISKLABEL} ${VND} ${DISKTYPE}
.endif
	newfs ${NEWFSOPTS} ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
.ifdef BOOT_INSTALLBOOT
	/usr/mdec/installboot -v ${MOUNT_POINT}/boot ${BOOT_INSTALLBOOT} ${VND}
.endif
	cd ${DESTDIR} && tar cf - `cat ${LIST}` | ( cd ${MOUNT_POINT}; \
	  tar xpf - )
	cp ${.CURDIR}/install.sh ${MOUNT_POINT}/install
	chmod 755 ${MOUNT_POINT}/install
	sed "/^VERSION=/s/=.*/=${REV}/" < ${.CURDIR}/install.sub \
	  > ${MOUNT_POINT}/install.sub
	cp ${.CURDIR}/../${MACHINE}/install.md ${MOUNT_POINT}/install.md
	mkdir -p ${MOUNT_POINT}/kern
	mkdir -p ${MOUNT_POINT}/mnt2
.if exists(/sbin/ldconfig)
	/usr/sbin/chroot ${MOUNT_POINT} /sbin/ldconfig
.endif
	( cd ${MOUNT_POINT} && \
	tar cf - .??* * | ${GZIP} ${GZIPFLAGS} \
	  > ${IMAGEDIR}/simpleroot${REV}.tar${GZIPEXT} )
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
.if (${GZIP} == "gzip")
	${GZIP} ${IMAGE}
.endif
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND_DEV}
	-/bin/rm -f ${IMAGE}

clean cleandir:
	/bin/rm -f core ${IMAGE}

beforeinstall:
	cp ${IMAGE} ${DESTDIR}/snapshot

.endif	# DESTDIR check
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
