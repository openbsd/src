#	$OpenBSD: Makefile,v 1.1.1.1 1998/06/23 18:46:42 mickey Exp $
#	$NOWHERE: Makefile,v 2.1 1998/06/22 19:34:45 mickey Exp $

.include "${.CURDIR}/../Makefile.inc"

LIB=	sa

S=${.CURDIR}/../../../..
SADIR=${.CURDIR}/..
DIR_SA=	$S/lib/libsa

NOPROFILE=noprofile
NOPIC=nopic

#AS=cat ; 
#AS+=	-R
#AS+=	-a

# hppa stuff
SRCS=	machdep.c pdc.c itecons.c dk.c dev_hppa.c exec_hppa.c time.c \
	ct.c dk.c cache_s.s cache_c.c

# stand routines
SRCS+=	alloc.c exit.c exec.c getfile.c gets.c globals.c strcmp.c strlen.c \
	strncmp.c memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c \
	strtol.c ctime.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c readdir.c cons.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c

.PATH:  ${DIR_SA}

all: ${SALIB}

install:

.include <bsd.lib.mk>

CPPFLAGS+=-DHEAP_LIMIT=${HEAP_LIMIT} ${DEBUGFLAGS}
CPPFLAGS+=-I${S}/stand/boot
CFLAGS+=${SACFLAGS} -D__INTERNAL_LIBSA_CREAD
