#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		miniroot-${REV}.fs
CRUNCHCONF?=    ${.CURDIR}/${CBIN}.conf
BASE=miniroot

MOUNT_POINT=	/mnt1
BDEV=		/dev/rd0a
CDEV=		/dev/rrd0a
RDEV=		/dev/rd0c

KERNEL=		${SRCSYSDIR}/arch/mvme68k/compile/MINIROOT/bsd

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

rdsetroot:	${TOP}/common/rdsetroot.c
	${HOSTCC} -o rdsetroot ${TOP}/common/rdsetroot.c

setroot: ${IMAGE} bsd 
	rdsetroot bsd < ${IMAGE}

bsd:	${KERNEL}
	cp ${KERNEL} bsd

writetape:
	echo rewinding tape < /dev/rst0
	buffer -i /usr/mdec/stboot -o /dev/nrst0
	buffer -i /usr/mdec/bootst -o /dev/nrst0
	buffer -B -p75 -i bsd -o /dev/nrst0
	echo rewinding tape < /dev/rst0


do_prep: ${CBIN} do_unconfig
	sh ${.CURDIR}/start_rdconfig.sh ${RDEV} ${NBLKS}
	disklabel -w ${RDEV} ${DISKTYPE}
	
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
	if [ -e rd.pid ] ; then \
	kill `< rd.pid`; \
	rm rd.pid; \
	fi


.PRECIOUS:	${IMAGE}


