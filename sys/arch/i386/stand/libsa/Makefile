#	$OpenBSD: Makefile,v 1.47 2007/05/30 01:25:43 tom Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	sa

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..

# i386 stuff (so, it will possibly load in the same 64k)
SRCS+=	machdep.c dev_i386.c exec_i386.c cmd_i386.c loadfile.c elf32.c elf64.c

.if defined(DEBUGFLAGS) && !empty(DEBUGFLAGS:M-D_TEST)
SRCS+=	unixdev.c unixsys.S nullfs.c memprobe.c
CLEANFILES+=	gidt.o debug_i386.o alloca.o \
	biosdev.o bioscons.o gateA20.o apmprobe.o \
	cpuprobe.o memprobe.o diskprobe.o smpprobe.o \
	time.o biosprobe.o
.else
SRCS+=	gidt.S debug_i386.S alloca.S \
	biosdev.c bioscons.c gateA20.c apmprobe.c \
	cpuprobe.c memprobe.c diskprobe.c pciprobe.c smpprobe.c \
	time.c biosprobe.c ps2probe.c pxe.c pxe_net.c pxe_call.S
CLEANFILES+=	unixdev.o unixsys.o nullfs.o
.endif

# stand routines
SRCS+=	alloc.c exit.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c snprintf.c \
	strerror.c strncpy.c strtol.c strtoll.c ctime.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c readdir.c cons.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c tftp.c cd9660.c

# debugger
SRCS+=	debug.c

NOPROFILE=noprofile
NOPIC=nopic

.PATH:  $S/lib/libsa

all: ${SALIB}

install:

.include <bsd.lib.mk>

CPPFLAGS+=-DLINKADDR=${LINKADDR} -DHEAP_LIMIT=${HEAP_LIMIT} ${DEBUGFLAGS}
CPPFLAGS+=-I${S}/stand/boot #-DCOMPAT_UFS
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
#AS=cat ; 
#AS+=	-R
#AS+=	-a
