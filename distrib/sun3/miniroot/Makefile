#	$NetBSD: Makefile,v 1.2 1995/10/13 16:46:47 gwr Exp $

TOP=		${.CURDIR}/..

# This include just sets REV=XX
.include "${TOP}/Makefile.inc"

IMAGE=	miniroot
CBIN=	mini_usr

TREE=	${TOP}/common/${IMAGE}.tree

LISTS=	${TOP}/common/mini_sbin.list \
	${TOP}/common/mini_bin.list \
	${TOP}/common/${CBIN}.list \
	${TOP}/common/${IMAGE}.list

MOUNT_POINT=	/mnt
DISKTYPE=	miniroot
BDEV=		/dev/sd2b
CDEV=		/dev/rsd2b

KERNEL=${SRCSYSDIR}/arch/sun3/compile/GENERIC/netbsd

# These are all the parameters for the miniroot: (6MB)
NBLKS=		12288
# old format, minfree, opt,  b/i  trks, sects, cpg
NEWFSARGS= -O -m 0 -o space -i 8192 -t 8 -u 32 -c 16

${IMAGE}.fs: ${CBIN} do_mount do_files do_bootxx do_kernel do_umount
	dd if=${CDEV} of=$@ bs=64b count=`expr ${NBLKS} / 64`

# Only do these for the miniroot, not the ramdisk.
do_bootxx:
	sync ; sleep 1 ; sync
	cd ${MOUNT_POINT} ;\
	  usr/mdec/installboot -v ufsboot usr/mdec/bootxx ${CDEV}

# Copy the generic kernel
do_kernel:
	cp -p ${KERNEL} ${MOUNT_POINT}/netbsd

# Do not delete this if I change my mind and kill make...
.PRECIOUS: ${IMAGE}.fs

# Rules used making ${IMAGE}.fs (do_*)
.include "${TOP}/common/Make.fsimage"

# Rules for making ${CBIN} ...
.include "${TOP}/common/Make.crunch"

clean cleandir:
	-rm -f a.out core *.core *.o
	-rm -f ${CLEANFILES}

# Standard rules needed by the above...
.include <bsd.obj.mk>
