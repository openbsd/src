#	$OpenBSD: Makefile,v 1.28 2011/04/05 12:50:15 guenther Exp $

# Makefile for kernel tags files, init_sysent, etc.

ARCH=	alpha amd64 armish aviion beagle hp300 \
	hppa hppa64 i386 landisk loongson \
	luna88k mac68k macppc mvme68k \
	mvme88k mvmeppc palm sgi socppc \
	solbourne sparc sparc64 vax zaurus

all:	init_sysent.c

SYSCALLSRC = makesyscalls.sh syscalls.conf syscalls.master
init_sysent.c syscalls.c ../sys/syscall.h ../sys/syscallargs.h: ${SYSCALLSRC}
	sh makesyscalls.sh syscalls.conf syscalls.master

# Kernel tags:
# tags files are built in the top-level directory for each architecture.
# Links to the correct tags file are placed in each source directory.
# We need to have links to tags files
# from the generic directories that are relative to the machine type, even
# via remote mounts; therefore we use symlinks to $SYSTAGS, which points at
# ${SYSDIR}/${MACHINE}/tags.

SYSTAGS=/var/db/sys_tags
SYSDIR=/sys

# Directories in which to place tags links (other than machine-dependent)
DGEN=	compat compat/common \
	compat/linux compat/svr4 \
	conf \
	ddb \
	dev dev/eisa dev/ic dev/isa dev/pci dev/pcmcia dev/cardbus \
	dev/sun dev/tc \
	isofs isofs/cd9660 \
	kern \
	lib/libkern lib/libkern/arch/${MACHINE} \
	lib/libkern/arch/${MACHINE_ARCH} \
	miscfs miscfs/deadfs miscfs/fifofs \
	miscfs/portal miscfs/procfs miscfs/specfs \
	msdosfs \
	net netinet netmpls \
	nfs \
	scsi \
	sys \
	ufs ufs/ffs ufs/mfs ufs/ufs \
	uvm

tags::
	-for i in ${ARCH}; do \
	    (cd ../arch/$$i && make ${MFLAGS} tags); done

links::
	rm -f ${SYSTAGS}
	ln -s ${SYSDIR}/arch/${MACHINE}/tags ${SYSTAGS}
	-for i in ${DGEN}; do \
	    (cd ../$$i && { rm -f tags; ln -s ${SYSTAGS} tags; }) done
	-for i in ${ARCH}; do \
	    (cd ../arch/$$i && make ${MFLAGS} SYSTAGS=${SYSTAGS} links); done
