#	$NetBSD: Makefile,v 1.2 1995/10/13 16:51:30 gwr Exp $

TOP=		${.CURDIR}/..

# This include just sets REV=XX
.include "${TOP}/Makefile.inc"

IMAGE=	ramdisk
CBIN=	rd_bin

TREE=	${TOP}/common/${IMAGE}.tree

LISTS=	${TOP}/common/${CBIN}.list \
	${TOP}/common/${IMAGE}.list

MOUNT_POINT=	/mnt
DISKTYPE=	rdroot
BDEV=		/dev/rd0
CDEV?=		/dev/rd0

KERNEL=${SRCSYSDIR}/arch/sun3/compile/RAMDISK/netbsd

# These are all the parameters for the root fs: (256K)
NBLKS=		512
# args: old fmt, minfree, opt, b/i  trks, sects, cpg
NEWFSARGS= -O -m 0 -o space -i 2048 -t 2 -u 16 -c 16

CLEANFILES= netbsd-rd rdsetroot

netbsd-rd : rdsetroot ${IMAGE}.fs
	cp ${KERNEL} netbsd-tmp
	./rdsetroot  netbsd-tmp < ${IMAGE}.fs
	-mv -f netbsd-tmp $@

rdsetroot: ${TOP}/common/rdsetroot.c
	$(CC) -o $@ -DDEBUG ${TOP}/common/rdsetroot.c

${IMAGE}.fs: ${CBIN} do_mount do_files do_umount
	dd if=${CDEV} of=$@ count=${NBLKS}

# Do not delete this if I change my mind and kill make...
.PRECIOUS: ${IMAGE}.fs

# Rules used making ${IMAGE}.fs (do_*)
.include "${TOP}/common/Make.fsimage"

# Rules for making ${CBIN} ...
.include "${TOP}/common/Make.crunch"

${CBIN} : libhack.o

# Use stubs to eliminate some large stuff from libc
HACKSRC=${TOP}/../utils/libhack
.include "${HACKSRC}/Makefile.inc"

clean cleandir:
	-rm -f a.out core *.core *.o
	-rm -f ${CLEANFILES}

# Standard rules needed by the above...
.include <bsd.obj.mk>
