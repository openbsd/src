#	$OpenBSD: Makefile.inc,v 1.15 2004/11/25 22:02:08 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CBIN?=		instbin
CRUNCHCONF?=	${CBIN}.conf
LISTS?=		${.CURDIR}/../common/list
UTILS?=		${.CURDIR}/../../miniroot

MOUNT_POINT=	/mnt
MTREE=		${UTILS}/mtree.conf

XNAME?=		floppy
FS?=		${XNAME}${REV}.fs
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
BOOT=		${DESTDIR}/usr/mdec/boot
FLOPPYSIZE?=	144
FLOPPYTYPE?=	floppy3

all:	${FS}

${FS}:	bsd.gz
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=${FLOPPYSIZE}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${FLOPPYTYPE}
	newfs -m 0 -o space -i 524288 -c 80 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	cp ${BOOT} ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	strip -R .comment ${.OBJDIR}/boot
	dd if=${.OBJDIR}/boot of=${MOUNT_POINT}/boot bs=512
	dd if=bsd.gz of=${MOUNT_POINT}/bsd bs=512
	/usr/mdec/installboot -v ${MOUNT_POINT}/boot \
	    ${DESTDIR}/usr/mdec/biosboot ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${FS}
	rm ${REALIMAGE}

DISKTYPE?=       rdroot
NBLKS?=          3800
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -c 16 -i 4096

bsd.gz: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	strip -R .comment bsd.strip
	gzip -c9 bsd.strip > bsd.gz

bsd.rd:	${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}


bsd:
	cd ${.CURDIR}/../../../sys/arch/i386/conf && config ${RAMDISK}
	cd ${.CURDIR}/../../../sys/arch/i386/compile/${RAMDISK} && \
	    make clean && make depend && COPTS=-Os make
	cp ${.CURDIR}/../../../sys/arch/i386/compile/${RAMDISK}/bsd bsd

${IMAGE}: ${CBIN} rd_setup do_files rd_teardown

rd_setup: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=512 count=${NBLKS}
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} ${DISKTYPE}
	newfs ${NEWFSARGS} ${VND_RDEV}
	fsck ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}

rd_teardown:
	@df -i ${MOUNT_POINT}
	-umount ${MOUNT_POINT}
	-vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

rdsetroot:	${TOP}/../common/elfrdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/../common/elfrdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}

.PRECIOUS:	${IMAGE}

install:
.ifndef NOBSDRD
	cp bsd.rd ${DESTDIR}/snapshot/bsd.rd
.endif
	cp ${FS} ${DESTDIR}/snapshot/${FS}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c ${CBIN}.c -e ${CBIN} -m ${CBIN}.mk ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all
	strip -R .comment ${CBIN}

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CRUNCHCONF}

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} UTILS=${UTILS} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm ${MOUNT_POINT}/${CBIN}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}*.cache \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs \
	    rdsetroot boot ${CRUNCHCONF} ${FS}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
