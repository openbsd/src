#	$OpenBSD: Makefile,v 1.3 1996/04/25 21:32:01 niklas Exp $
#	$NetBSD: Makefile,v 1.3 1995/11/21 21:19:14 gwr Exp $

TOP=		${.CURDIR}/..

# This include just sets REV=XX
.include "${TOP}/Makefile.inc"

IMAGE=	ramdisk
CBIN=	rd_bin

TREE=	${TOP}/common/${IMAGE}.tree

LISTS=	${TOP}/common/${CBIN}.list \
	${TOP}/common/${IMAGE}.list

KERNEL=${SRCSYSDIR}/arch/sun3/compile/RAMDISK/bsd

MOUNT_POINT=	/mnt
BDEV=		/dev/rd0
CDEV=		/dev/rd0

# These are all the parameters for the root fs: (256K)
DISKTYPE=	rdroot
NBLKS=		512
# old format, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -O -m 0 -o space -i 2048 -t 2 -u 16 -c 16

CLEANFILES= bsd-rd rdsetroot ${IMAGE}.fs

bsd-rd : rdsetroot ${IMAGE}.fs
	cp ${KERNEL} bsd-tmp
	./rdsetroot  bsd-tmp < ${IMAGE}.fs
	-mv -f bsd-tmp $@

rdsetroot: ${TOP}/common/rdsetroot.c
	$(CC) -o $@ -DDEBUG ${TOP}/common/rdsetroot.c

${IMAGE}.fs: ${TREE} ${LISTS} ${CBIN}
	-newfs ${NEWFSARGS} -s ${NBLKS} ${CDEV} ${DISKTYPE}
	mount ${BDEV} ${MOUNT_POINT}
	mtree -def ${TREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} \
	  OBJDIR=${.OBJDIR} TARGDIR=${MOUNT_POINT} \
	  sh ${TOP}/common/RunList.sh ${LISTS}
	sync
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	-umount ${MOUNT_POINT}
	dd if=${CDEV} of=$@ count=${NBLKS}

# Do not delete this if I change my mind and kill make...
.PRECIOUS: ${IMAGE}.fs

# Rules for making ${CBIN} ...
.include "${TOP}/common/Make.crunch"

# This is listed in rd_bin.conf but is built here.
${CBIN} : libhack.o

# Use stubs to eliminate some large stuff from libc
HACKSRC=${TOP}/../utils/libhack
.include "${HACKSRC}/Makefile.inc"

clean cleandir:
	-rm -f a.out core *.core *.o
	-rm -f ${CLEANFILES}

# Standard rules needed by the above...
.include <bsd.obj.mk>
