#	$OpenBSD: Makefile,v 1.31 1998/12/21 10:10:15 deraadt Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		mr.fs
CBIN?=		instbin
.if (${KERBEROS} == "yes")
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
.else
CRUNCHCONF?=    ${.CURDIR}/${CBIN}-nokrb.conf
.endif
LISTS=		${.CURDIR}/list

MOUNT_POINT=	/mnt
MTREE=		${.CURDIR}/mtree.conf

FS=		floppy${REV}.fs
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
LISTFLOPPY=	${.CURDIR}/list_floppy

all:	${FS}

${FS}:	bsd.gz
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} floppy3
	newfs -m 0 -o space -i 524288 -c 80 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTFLOPPY}
	/usr/mdec/installboot -v -s 18 -h 2 ${MOUNT_POINT}/boot \
	    ${DESTDIR}/usr/mdec/biosboot ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${FS}
	rm ${REALIMAGE}

DISKTYPE=       rdroot
NBLKS=          3560
# minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -m 0 -o space -c 16 -i 4096

bsd.gz: bsd.rd
	cp bsd.rd bsd.strip
	strip bsd.strip
	gzip -c9 bsd.strip > bsd.gz

bsd.rd:	${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.OBJDIR}/rdsetroot bsd.rd < ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../../sys/arch/i386/conf && config RAMDISK
	cd ${.CURDIR}/../../../../sys/arch/i386/compile/RAMDISK && \
	    make clean && make
	cp ${.CURDIR}/../../../../sys/arch/i386/compile/RAMDISK/bsd bsd

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

rdsetroot:	${TOP}/common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/common/rdsetroot.c

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.PRECIOUS:	${IMAGE}

install:
	cp bsd.rd ${DESTDIR}/snapshot/bsd.rd
	cp ${FS} ${DESTDIR}/snapshot/${FS}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib \
	-c ${CBIN}.c -e ${CBIN} -m ${CBIN}.mk ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all

do_files:
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    REV=${REV} TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}

clean cleandir:
	/bin/rm -f core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}*.cache \
	    *.o *.lo *.c bsd bsd.rd bsd.gz bsd.strip floppy*.fs rdsetroot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
