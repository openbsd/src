#	$NetBSD: Makefile,v 1.2 1995/09/02 05:04:25 thorpej Exp $

LIB=	sa

.PATH:	${.CURDIR}/../../../../lib/libsa

# Don't need these now...
# DEBUGFLAGS=-DNETIF_DEBUG -DRPC_DEBUG -DNFS_DEBUG -DRARP_DEBUG -DNET_DEBUG

CFLAGS+=-DSTANDALONE -DCOMPAT_UFS ${DEBUGFLAGS}
CFLAGS+=-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libsa

# stand routines
SRCS=	alloc.c bcopy.c exit.c exec.c getfile.c gets.c globals.c \
	printf.c strerror.c

# io routines
SRCS+=	close.c dev.c disklabel.c ioctl.c lseek.c open.c read.c \
	stat.c write.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c

NOPROFILE=
NOPIC=
OBJMACHINE=

install:

.include <bsd.lib.mk>
