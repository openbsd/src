#	$OpenBSD: Makefile.inc,v 1.22 2019/04/29 01:48:41 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CRUNCHCONF?=    ${.CURDIR}/../common/instbin.conf
LISTS?=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

XNAME?=		floppy
FS?=		${XNAME}${OSrev}.fs
LISTFLOPPY?=	${.CURDIR}/../common/list_floppy
FLOPPYSIZE?=	144
FLOPPYSECS?=	18
FLOPPYTYPE?=	floppy3

all:	${FS}

${FS}:	bsd.rd
	dd if=/dev/zero of=${FS} bs=10k count=${FLOPPYSIZE}
	vnconfig -v ${FS} > vnd
	disklabel -w `cat vnd` ${FLOPPYTYPE}
	newfs -m 0 -o space -i 524288 -c ${FLOPPYSIZE} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTFLOPPY}
	installboot -v `cat vnd`
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u `cat vnd`
	rm -f vnd

DISKTYPE?=       rdroot
NBLKS?=          3510
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -i 4096

${IMAGE}: instbin rd_setup do_files rd_teardown

rd_setup: instbin
	dd if=/dev/zero of=${IMAGE} bs=512 count=${NBLKS}
	vnconfig -v ${IMAGE} > vnd
	disklabel -w `cat vnd` ${DISKTYPE}
	newfs ${NEWFSARGS} /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}

rd_teardown:
	df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u `cat vnd`

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.PRECIOUS:	${IMAGE}

.ifdef RELEASEDIR
install:
.ifndef NOBSDRD
	cp bsd.rd ${RELEASEDIR}
	chmod a+r ${RELEASEDIR}/bsd.rd
.endif
	cp ${FS} ${RELEASEDIR}
.endif

instbin.mk instbin.cache instbin.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c instbin.c -e instbin -m instbin.mk ${CRUNCHCONF}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    OSrev=${OSrev} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    RELEASEDIR=${RELEASEDIR} sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/instbin

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin*.cache \
	    lib*.a lib*.olist instbin.map \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs

.include <bsd.subdir.mk>
