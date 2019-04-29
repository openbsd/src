#	$OpenBSD: Makefile,v 1.44 2019/04/29 01:48:41 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

FLOPPY=		floppy${OSrev}.fs
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${FLOPPY} bsd.rd

bsd.gz:	bsd.rd
	gzip -9nv < bsd.rd > bsd.gz

${FLOPPY}:	bsd.gz ${BOOT} /usr/
	dd if=/dev/zero of=${FLOPPY} count=${FLOPPYSIZE}
	vnconfig -v ${FLOPPY} > vnd
	disklabel -w `cat vnd` ${FLOPPYTYPE}
	newfs -m 0 -o space -i 524288 -c ${FLOPPYSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	cp ${DESTDIR}/usr/mdec/ofwbootfd ${MOUNT_POINT}/ofwboot
	cp bsd.gz ${MOUNT_POINT}/bsd
	installboot -v `cat vnd` /usr/mdec/bootblk ${MOUNT_POINT}/ofwboot
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

bsd.rd: instbin ${IMAGE} bsd
	cp bsd bsd.rd
	rdsetroot bsd.rd ${IMAGE}
	strip -R .SUNW_ctf bsd.rd

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

bsd:
	cd ${TOP}/../../sys/arch/sparc64/compile/RAMDISKU1 && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && env ${MAKE}'
	cp -p ${TOP}/../../sys/arch/sparc64/compile/RAMDISKU1/obj/bsd bsd

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map \
	    *.o *.lo *.c bsd bsd.rd bsd.gz ${FLOPPY}
	/bin/rm -rf ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${FLOPPY} ${BSD.RD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
