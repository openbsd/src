#	$OpenBSD: Makefile,v 1.2 1997/04/14 10:57:09 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		miniroot-${REV}.fs
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
BASE=miniroot

MOUNT_POINT=	/mnt
BDEV=		/dev/rd0a
CDEV=		/dev/rrd0a
RDEV=		/dev/rd0c

VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
VND_CRDEV=	/dev/r${VND}c
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}
LISTFLOPPY=	${.CURDIR}/list_floppy

.include "Makefile.inc"

all:	floppy

floppy: bsd.gz
.ifndef FLOPPY3
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=120
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} floppy5
	newfs -m 0 -o space -i 8192 -c 80 ${VND_RDEV} floppy5
.else
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -r ${VND} floppy3
	newfs -m 0 -o space -i 6500 -c 80 ${VND_RDEV} floppy3
.endif
	mount ${VND_DEV} ${MOUNT_POINT}
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTFLOPPY}
	sync
	cat /*bin/* > /dev/null
	sync
	/usr/mdec/installboot -v /mnt/boot ${DESTDIR}/usr/mdec/biosboot ${VND_CRDEV}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	sync
	cat /*bin/* > /dev/null
	sync
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

DISKTYPE=       rdroot
NBLKS=          4046
# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -t ffs -m 0 -o space -c 16 -i 4096

bsd.gz: bsd.rd
	gzip -c9 bsd.rd > bsd.gz

bsd.rd: ${IMAGE} bsd rdsetroot
	cp bsd bsd.rd
	${.CURDIR}/${__objdir}/rdsetroot bsd.rd < ${IMAGE}

bsd:
	cd ${.CURDIR}/../../../../sys/arch/i386/conf && config RAMDISK
	cd ${.CURDIR}/../../../../sys/arch/i386/compile/RAMDISK && \
	    make clean && make depend && make
	cp ${.CURDIR}/../../../../sys/arch/i386/compile/RAMDISK/bsd bsd

${IMAGE}:	do_prep do_mount do_files do_umount_copy do_unconfig ${CBIN}
	mv -f ${IMAGE}.tmp ${IMAGE}
	-if [ -e ${.CURDIR}/${__objdir}/rd.pid ] ; then \
	kill `cat ${.CURDIR}/${__objdir}/rd.pid`; \
	rm ${.CURDIR}/${__objdir}/rd.pid;\
	fi

rdsetroot:	${TOP}/common/rdsetroot.c
	${HOSTCC} -DDEBUG -o rdsetroot ${TOP}/common/rdsetroot.c

writetape:
	echo rewinding tape < /dev/rst0
	buffer -i /usr/mdec/stboot -o /dev/nrst0
	buffer -i /usr/mdec/bootst -o /dev/nrst0
	buffer -B -p75 -i bsd -o /dev/nrst0
	echo rewinding tape < /dev/rst0


do_prep: ${CBIN} do_unconfig
	sh ${.CURDIR}/start_rdconfig.sh ${RDEV} ${NBLKS}
	disklabel -w -r ${RDEV} ${DISKTYPE}
	
do_mount:
	-newfs ${NEWFSARGS} -s ${NBLKS} ${CDEV} ${DISKTYPE}
	mount ${BDEV} ${MOUNT_POINT}
 
do_umount_copy:
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	-umount ${MOUNT_POINT}
	dd if=${CDEV} of=${IMAGE}.tmp bs=4b count=`expr ${NBLKS} / 4 `

do_unconfig:
	-umount ${MOUNT_POINT}
	-if [ -e ${.CURDIR}/${__objdir}/rd.pid ] ; then \
	kill `cat ${.CURDIR}/${__objdir}/rd.pid`; \
	rm ${.CURDIR}/${__objdir}/rd.pid; \
	fi

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

.PRECIOUS:	${IMAGE}


