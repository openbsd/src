#	$OpenBSD: Makefile,v 1.13 1997/06/23 23:45:17 kstailey Exp $


LIB=	sa

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..
DIR_SA=	$S/lib/libsa
DIR_KERN=$S/lib/libkern

CFLAGS+=$(SACFLAGS) -D__INTERNAL_LIBSA_CREAD
CFLAGS+=-DSTART=$(START) -DHEAP_LIMIT=$(HEAP_LIMIT)
CFLAGS+=${DEBUGFLAGS} -I${.CURDIR} -I${.CURDIR}/..
CFLAGS+=-I$(S)/lib/libsa -I$(S)
AS+=	-R
#AS+=	-Wa,-a

# i386 stuff (so, it will possibly load in the same 64k)
SRCS=	unixsys.S bioscom.S biosdisk.S bioskbd.S biostime.S biosmem.S gidt.S \
	debug_i386.S dev_i386.c exec_i386.c biosdev.c gateA20.c memprobe.c \
	time.c alloca.S

# stand routines
SRCS+=	alloc.c exit.c exec.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c

# math for cd9660
SRCS+=	divdi3.c qdivrem.c

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
SRCS+=	debug.c debug_aout.c

NOPROFILE=noprofile
NOPIC=nopic

.PATH:  ${DIR_SA} ${DIR_KERN}

all: machine-links ${SALIB}

machine-links:
	@rm -f machine i386
	@ln -s ${S}/arch/i386 i386
	@ln -s ${S}/arch/i386/include machine

install:

.include <bsd.lib.mk>
