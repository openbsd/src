#	$NetBSD: Makefile,v 1.10 1995/10/01 06:00:32 phil Exp $

LIB=	sa

DIR=${SAREL}${SADIR}

.PATH: ${DIR}

#DEBUGFLAGS= -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG -DDEBUG -DPARANOID -Wall
#DEBUGFLAGS= -ansi -pedantic -Wall
CFLAGS+=-DSTANDALONE -DCOMPAT_UFS $(DEBUGFLAGS) -I${DIR} -I${DIR}/../..

# stand routines
SRCS+=	alloc.c bcopy.c exit.c exec.c getfile.c gets.c globals.c \
	printf.c strerror.c

# io routines
SRCS+=	close.c dev.c disklabel.c ioctl.c lseek.c open.c read.c \
	stat.c write.c

.if !defined(NO_NET)
# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c
.endif

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	ufs.c nfs.c

NOPROFILE=
NOPIC=
OBJMACHINE=

install:

.include <bsd.lib.mk>
