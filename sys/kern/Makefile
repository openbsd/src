#	$OpenBSD: Makefile,v 1.10 2002/12/31 16:22:26 miod Exp $

# Makefile for kernel tags files, init_sysent, etc.

ARCH=	alpha hp300 hppa i386 m68k mac68k mvme68k mvme88k \
	mvmeppc powerpc sparc sparc64 vax

all:	init_sysent.c vnode_if.c

SYSCALLSRC = makesyscalls.sh syscalls.conf syscalls.master
init_sysent.c syscalls.c ../sys/syscall.h ../sys/syscallargs.h: ${SYSCALLSRC}
	sh makesyscalls.sh syscalls.conf syscalls.master

VNODEIFSRC = vnode_if.sh vnode_if.src
vnode_if.c ../sys/vnode_if.h: ${VNODEIFSRC}
	sh vnode_if.sh vnode_if.src

# Kernel tags:
# Tags files are built in the top-level directory for each architecture,
# with a makefile listing the architecture-dependent files, etc.  The list
# of common files is in ./Make.tags.inc.  Links to the correct tags file
# are placed in each source directory.  We need to have links to tags files
# from the generic directories that are relative to the machine type, even
# via remote mounts; therefore we use symlinks to $SYSTAGS, which points at
# ${SYSDIR}/${MACHINE}/tags.

SYSTAGS=/var/db/sys_tags
SYSDIR=/sys

# Directories in which to place tags links (other than machine-dependent)
DGEN=	adosfs \
	compat compat/common compat/freebsd compat/hpux compat/ibcs2 \
	compat/linux compat/osf1 compat/sunos compat/svr4 compat/ultrix \
	conf \
	ddb \
	dev dev/eisa dev/ic dev/isa dev/pci dev/pcmcia dev/cardbus dev/rcons \
	dev/sun dev/tc \
	gnu \
	isofs isofs/cd9660 \
	kern \
	lib libkern \
	miscfs miscfs/deadfs miscfs/fdesc miscfs/fifofs miscfs/kernfs \
	miscfs/nullfs miscfs/portal miscfs/procfs \
	miscfs/specfs miscfs/umapfs miscfs/union \
	msdosfs \
	net netccitt netinet netiso netns \
	nfs \
	scsi \
	sys \
	ufs ufs/ffs ufs/lfs ufs/mfs ufs/ufs \
	vm

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
