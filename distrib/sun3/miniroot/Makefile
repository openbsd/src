#	$NetBSD: Makefile,v 1.3 1995/11/21 21:19:12 gwr Exp $

TOP=		${.CURDIR}/..

# This include just sets REV=XX
.include "${TOP}/Makefile.inc"

IMAGE=	miniroot
CBIN=	mini_usr

TREE=	${TOP}/common/${IMAGE}.tree

LISTS=	${TOP}/common/mini_sbin.list \
	${TOP}/common/mini_bin.list \
	${TOP}/common/${CBIN}.list \
	${TOP}/common/mini_xx.list \
	${TOP}/common/${IMAGE}.list

KERNEL=${SRCSYSDIR}/arch/sun3/compile/GENERIC/netbsd

MOUNT_POINT=	/mnt
#BDEV=		/dev/sd2b
#CDEV=		/dev/rsd2b
BDEV=		/dev/sd0d
CDEV=		/dev/rsd0d

# These are all the parameters for the miniroot: (8MB)
DISKTYPE=	miniroot
NBLKS=		16384
# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -O -m 0 -o space -i 8192 -t 8 -u 32 -c 16

CLEANFILES= ${IMAGE}.gz

${IMAGE}.gz: ${TREE} ${LISTS} ${CBIN} mini_xx
	-newfs ${NEWFSARGS} -s ${NBLKS} ${CDEV} ${DISKTYPE}
	mount ${BDEV} ${MOUNT_POINT}
	mtree -def ${TREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} \
	  OBJDIR=${.OBJDIR} TARGDIR=${MOUNT_POINT} \
	  sh ${TOP}/common/RunList.sh ${LISTS}
	sync ; sleep 1 ; sync
	cd ${MOUNT_POINT} ;\
	  usr/mdec/installboot -v ufsboot usr/mdec/bootxx ${CDEV}
	cp ${KERNEL} ${MOUNT_POINT}/netbsd
	sync
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	-umount ${MOUNT_POINT}
	dd if=${CDEV} bs=16b count=`expr ${NBLKS} / 16` |\
	  gzip > ${IMAGE}.tmp
	-mv -f ${IMAGE}.tmp ${IMAGE}.gz

# Do not delete this if I change my mind and kill make...
.PRECIOUS: ${IMAGE}.gz

# Rules for making ${CBIN} ...
.include "${TOP}/common/Make.crunch"

clean cleandir: clean_xx
	-rm -f a.out core *.core *.o
	-rm -f ${CLEANFILES}

# Standard rules needed by the above...
.include <bsd.obj.mk>

# Hacks for re-linking some program -static
.include "${TOP}/common/mini_xx.mk"
