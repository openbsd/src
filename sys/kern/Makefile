#	@(#)Makefile	8.2 (Berkeley) 3/21/94

# Makefile for kernel tags files, init_sysent, etc.

ARCH=	alpha amiga atari hp300 i386 m68k mac68k mvme68k mvme88k pc532 \
	pica pmax sparc sun3 vax

all:
	@echo "make tags, make links or init_sysent.c only"

SYSCALLSRC = makesyscalls.sh syscalls.conf syscalls.master
init_sysent.c syscalls.c ../sys/syscall.h: ${SYSCALLSRC}
	-mv -f init_sysent.c init_sysent.c.bak
	-mv -f syscalls.c syscalls.c.bak
	-mv -f ../sys/syscall.h ../sys/syscall.h.bak
	-mv -f ../sys/syscallargs.h ../sys/syscallargs.h.bak
	sh makesyscalls.sh syscalls.conf syscalls.master

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
	dev dev/eisa dev/ic dev/isa dev/pci dev/pcmcia dev/rcons dev/sun \
	dev/tc \
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
