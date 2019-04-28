#	$OpenBSD: Makefile,v 1.39 2019/04/28 14:48:11 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
BSD_RD=		bsd.rd
IMAGE=		mr.fs
CBIN?=		instbin
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

FLOPPY=		floppy${REV}.fs
FLOPPYSIZE?=	2880
FLOPPYTYPE?=	floppy

GZIPFLAGS?=	-9nv

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before making a ramdisk!
	@false
.else

# mix config is not needed.
all:	${FLOPPY} ${BSD_RD}

bsd.gz:	bsd.rd
	gzip ${GZIPFLAGS} < bsd.rd > bsd.gz

${FLOPPY}:	bsd.gz ${BOOT} /usr/
	dd if=/dev/zero of=${FLOPPY} count=${FLOPPYSIZE}
	vnconfig -v ${FLOPPY} > vnd
	disklabel -w `cat vnd` ${FLOPPYTYPE}
	newfs -m 0 -o space -i 524288 -c ${FLOPPYSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	cp ${DESTDIR}/usr/mdec/ofwbootfd ${MOUNT_POINT}/ofwboot
	cp bsd.gz ${MOUNT_POINT}/bsd
	installboot -v `cat vnd` /usr/mdec/bootblk ${MOUNT_POINT}/ofwboot
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

${BSD_RD}: ${CBIN} ${IMAGE} bsd
	cp bsd ${BSD_RD}
	rdsetroot ${BSD_RD} ${IMAGE}
	strip -R .SUNW_ctf ${BSD_RD}

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

bsd:
	cd ${TOP}/../../sys/arch/sparc64/compile/RAMDISKU1 && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && env ${MAKE}'
	cp -p ${TOP}/../../sys/arch/sparc64/compile/RAMDISKU1/obj/bsd bsd

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

${CBIN}.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CBIN}.conf
	crunchgen -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    *.o *.lo *.c bsd ${BSD_RD} bsd.gz ${FLOPPY}
	/bin/rm -rf ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${FLOPPY} ${BSD.RD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
