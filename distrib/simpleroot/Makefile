# Revision is 2.1
REV=		21

ARCHDIR=	${.CURDIR}/../${MACHINE}/simpleroot

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
DISKTYPE=	simpleroot
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
IMAGE?=		/var/tmp/simpleroot${REV}.fs
NEWFSOPTS?=

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
	newfs ${NEWFSOPTS} ${VND_RDEV} ${DISKTYPE}
	mount ${VND_DEV} ${MOUNT_POINT}
.ifdef BOOT_INSTALLBOOT
	/usr/mdec/installboot -v ${MOUNT_POINT} ${BOOT_INSTALLBOOT} ${VND}
.endif
	cd ${DESTDIR} && tar cf - `cat ${LIST}` | ( cd ${MOUNT_POINT}; \
	  tar xpf - ) && \
	( cd ${MOUNT_POINT} && \
	tar cf - .??* * | ${GZIP} ${GZIPFLAGS} \
	  > /var/tmp/simpleroot${REV}.tar${GZIPEXT} )
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND_DEV}
	cat /*bin/* > /dev/null
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

.endif	# DESTDIR check
.include <bsd.obj.mk>
.include <bsd.subdir.mk>
