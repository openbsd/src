#	$OpenBSD: Makefile,v 1.2 1997/03/31 03:12:08 weingart Exp $


LIB= sa

CFLAGS+=-Wall
#CFLAGS+=-DBIOS_DEBUG
CFLAGS+=-DEXEC_DEBUG
CFLAGS+=-DNO_IDTR
CFLAGS+=-DHEAP_START="0x10000"
CFLAGS+=-DHEAP_LIMIT="0xa0000"
CFLAGS+=-D__INTERNAL_LIBSA_CREAD
CFLAGS+=${DEBUGFLAGS} -I${.CURDIR} -I${.CURDIR}/..
CFLAGS+=-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libsa
#AS+=	-Wa,-a

S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa
DIR_KERN=$S/lib/libkern

# stand routines
SRCS=   alloc.c exit.c exec.c getfile.c gets.c globals.c strcmp.c \
        strncmp.c memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c

# io routines
SRCS+=  close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
        open.c read.c stat.c write.c cread.c readdir.c cons.c

# network routines
SRCS+=  arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=  bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=  ufs.c nfs.c

# i386 stuff
SRCS+= asm.S bioscom.S biosdev.c biosdisk.S bioskbd.S biostime.S startprog.S \
	dev_i386.c exec_i386.c gateA20.c memprobe.c memsize.S probe_keyboard.c \
	real_prot.S srt0.S unixsys.S

NOPROFILE=noprofile
NOPIC=nopic

.PATH:  ${DIR_SA} ${DIR_KERN}

all: ${SALIB}

.include <bsd.lib.mk>

