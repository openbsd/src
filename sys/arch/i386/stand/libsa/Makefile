#	$OpenBSD: Makefile,v 1.30 1998/02/24 22:06:42 weingart Exp $


LIB=	sa

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..
DIR_SA=	$S/lib/libsa
DIR_KERN=$S/lib/libkern

#AS=cat ; 
#AS+=	-R
#AS+=	-a

# i386 stuff (so, it will possibly load in the same 64k)
SRCS=	gidt.S debug_i386.S alloca.S \
	machdep.c dev_i386.c exec_i386.c cmd_i386.c \
	biosdev.c bioscons.c gateA20.c apmprobe.c \
	memprobe.c diskprobe.c pciprobe.c smpprobe.c \
	time.c

# stand routines
SRCS+=	alloc.c exit.c exec.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c \
	strtol.c

# math for cd9660
#SRCS+=	divdi3.c qdivrem.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c readdir.c cons.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c

# debugger
SRCS+=	debug.c

NOPROFILE=noprofile
NOPIC=nopic

.PATH:  ${DIR_SA} ${DIR_KERN}

all: machine-links ${SALIB}

machine-links:
	@rm -f machine i386
	@ln -s ${.CURDIR}/../.. i386
	@ln -s ${.CURDIR}/../../include machine

install:

.include <bsd.lib.mk>

CPPFLAGS+=-DLINKADDR=${LINKADDR} -DHEAP_LIMIT=${HEAP_LIMIT} ${DEBUGFLAGS}
CPPFLAGS+=-I${S}/stand/boot
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
