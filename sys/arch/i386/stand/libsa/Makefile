#	$OpenBSD: Makefile,v 1.35 1998/07/20 18:14:55 mickey Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	sa

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..
DIR_SA=	$S/lib/libsa
DIR_KERN=$S/lib/libkern

# i386 stuff (so, it will possibly load in the same 64k)
SRCS+=	machdep.c dev_i386.c exec_i386.c cmd_i386.c

.if defined(DEBUGFLAGS) && !empty(DEBUGFLAGS:M-D_TEST)
SRCS+=	unixdev.c unixsys.S nullfs.c
CLEANFILES+=	gidt.o debug_i386.o alloca.o \
	biosdev.o bioscons.o gateA20.o apmprobe.o \
	memprobe.o diskprobe.o pciprobe.o smpprobe.o \
	time.o
.else
SRCS+=	gidt.S debug_i386.S alloca.S \
	biosdev.c bioscons.c gateA20.c apmprobe.c \
	memprobe.c diskprobe.c pciprobe.c smpprobe.c \
	time.c
CLEANFILES+=	unixdev.o unixsys.o nullfs.o
.endif

# stand routines
SRCS+=	alloc.c exit.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c \
	strtol.c ctime.c exec.new.c exec_aout.c

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

all: ${SALIB}

install:

.include <bsd.lib.mk>

CPPFLAGS+=-DLINKADDR=${LINKADDR} -DHEAP_LIMIT=${HEAP_LIMIT} ${DEBUGFLAGS}
CPPFLAGS+=-I${S}/stand/boot -DCOMPAT_UFS
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
#AS=cat ; 
#AS+=	-R
#AS+=	-a
