#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		miniroot-${REV}.fs
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
BASE=miniroot

MOUNT_POINT=	/mnt1
BDEV=		/dev/vnd0a
CDEV=		/dev/rvnd0a
RDEV=		/dev/vnd0c

all: ${CBIN} ${IMAGE}
	@echo "all done"

.include "Makefile.inc"

DISKTYPE=       rdroot
NBLKS=          4096
# old format, minfree, opt, b/i  trks, sects, cpg
#NEWFSARGS= -t ffs -m 0 -o space -u 32 -c 16

${IMAGE}:	do_prep do_mount do_files do_umount_copy do_unconfig
	mv -f ${IMAGE}.tmp ${IMAGE}
	if [ -e rd.pid ] ; then \
	kill `< rd.pid`; \
	rm rd.pid;\
	fi

do_prep: ${CBIN} do_unconfig
	#sh ${.CURDIR}/start_rdconfig.sh ${CDEV} ${NBLKS}
	dd if=/dev/zero of=${IMAGE} bs=1k count=2048
	vnconfig -cv ${RDEV} ${IMAGE}
	disklabel -r -w ${RDEV} ${DISKTYPE}
	
do_mount:
	-newfs ${NEWFSARGS} -s ${NBLKS} ${BDEV} ${DISKTYPE}
	mount ${BDEV} ${MOUNT_POINT}
 
do_umount_copy:
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	-umount ${MOUNT_POINT}
	dd if=${CDEV} of=${IMAGE}.tmp bs=4b count=`expr ${NBLKS} / 4 `

do_unconfig:
	-umount ${MOUNT_POINT}
	-vnconfig -u ${RDEV}

.PRECIOUS:	${IMAGE}


