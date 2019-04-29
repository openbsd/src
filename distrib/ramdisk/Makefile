#	$OpenBSD: Makefile,v 1.61 2019/04/29 17:55:54 naddy Exp $

ARCHDIR=	${.CURDIR}/../${MACHINE}/ramdisk

.include <bsd.own.mk>

.if exists(${ARCHDIR}/Makefile.inc)
.include "${ARCHDIR}/Makefile.inc"
.endif

MOUNT_POINT?=	/mnt
DISKTYPE=	rdroot
IMAGE?=		ramdisk${OSrev}.fs
IMAGESIZE?=	8192 # 4MB in 512 byte blocks
CRUNCHGENOPTS?=-E
UTILS?=		${.CURDIR}/../miniroot

LISTS=		${.CURDIR}/list ${ARCHDIR}/list.local
MTREE=		${UTILS}/mtree.conf

FLOPPY?=	floppy${OSrev}.fs
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy
INSTALLBOOT?=	installboot
BOOT?=		${DESTDIR}/usr/mdec/boot
BOOTIN?=	${MOUNT_POINT}/boot
BOOTXX?=	${DESTDIR}/usr/mdec/bootxx

# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSOPTS=	-t ffs -m 0 -o space -f 1024 -b 8192
NEWFSOPTS_RD?=	${NEWFSOPTS} -i 8192 -s ${IMAGESIZE}
#NEWFSOPTS_FD=	${NEWFSOPTS} -i 524288 -c ${FLOPPYSIZE} -s ${FLOPPYSIZE}
NEWFSOPTS_FD=	${NEWFSOPTS} -i 1048575 -c ${FLOPPYSIZE} -s ${FLOPPYSIZE}

.ifndef DESTDIR
all:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

.ifdef MAKEFLOPPY
all:	bsd.rd ${FLOPPY}
.else
all:	bsd.rd
.endif

${FLOPPY}:	bsd.gz ${BOOT} ${BOOTXX}
	dd if=/dev/zero of=${FLOPPY} count=${FLOPPYSIZE}
	vnconfig -v ${FLOPPY} > vnd
	disklabel -w `cat vnd` ${FLOPPYTYPE}
	newfs ${NEWFSOPTS_FD} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	cp bsd.gz ${MOUNT_POINT}/bsd
	${INSTALLBOOT} -v -r ${MOUNT_POINT} `cat vnd` ${BOOTXX} ${BOOT}
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

bsd:
	cd ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${RAMDISK} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && ${MAKE}'
	cp -p ${.CURDIR}/../../sys/arch/${MACHINE}/compile/${RAMDISK}/obj/bsd bsd

bsd.rd:	bsd ${IMAGE}
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}
	strip -R .SUNW_ctf bsd.rd

bsd.gz:	bsd.rd
	gzip -9nv < bsd.rd > bsd.gz

${IMAGE}: instbin
	dd if=/dev/zero of=${IMAGE} count=${IMAGESIZE}
	vnconfig -v ${IMAGE} > vnd
	disklabel -w `cat vnd` ${DISKTYPE}
	newfs ${NEWFSOPTS_RD} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	OSrev=${OSrev} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    ARCHDIR=${ARCHDIR} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/instbin
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen ${CRUNCHGENOPTS} -D ${.CURDIR}/../.. -L ${DESTDIR}/usr/lib \
	    instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map instbin.conf *.o *.lo *.c bsd bsd.*

.endif	# DESTDIR check

.ifdef RELEASEDIR
beforeinstall:
	cp bsd.rd ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.ifdef MAKEFLOPPY
	cp ${FLOPPY} ${RELEASEDIR}
.endif	# MAKEFLOPPY
.endif	# RELEASEDIR

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
