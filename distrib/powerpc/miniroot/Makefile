#	$NetBSD: Makefile,v 1.1 1995/07/18 04:13:06 briggs Exp $

TOP=		${.CURDIR}/..

.include "${TOP}/Makefile.inc"
IMAGE=		miniroot-${REV}.fs
CRUNCHCONF?=    ${TOP}/miniroot/${CBIN}.conf
BASE=miniroot
KERNEL=		${SRCSYSDIR}/arch/powerpc/compile/MINI/bsd

MOUNT_POINT=	/mnt1
BDEV=		/dev/rd0a
CDEV=		/dev/rrd0a
RDEV=		/dev/rd0c

.include "${TOP}/inst-common/Makefile.inc"

all: ${CBIN} ${IMAGE}
	@echo "all done"

DISKTYPE=       rdroot
NBLKS=          4096
# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -t ffs -m 0 -o space -c 16

${IMAGE}:	do_prep do_mount do_files do_umount_copy do_unconfig
	mv -f ${IMAGE}.tmp ${IMAGE}
	if [ -e rd.pid ] ; then \
	kill `< rd.pid`; \
	rm rd.pid;\
	fi

elfrdsetroot:	${TOP}//elfrdsetroot.c
	${HOSTCC} -o elfrdsetroot ${TOP}/elfrdsetroot.c

setroot: ${IMAGE} bsd elfrdsetroot
	${.OBJDIR}/elfrdsetroot bsd < ${IMAGE}

bsd:	${KERNEL}
	cp ${KERNEL} bsd

do_prep: ${CBIN} do_unconfig
	sh ${.CURDIR}/start_rdconfig.sh ${RDEV} ${NBLKS}
	dd if=/usr/mdec/sdboot of=${CDEV}
	disklabel -w ${CDEV} ${DISKTYPE}
	
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
	rm rd.pid;\
	fi


.PRECIOUS:	${IMAGE}


