#	$OpenBSD: Makefile,v 1.9 1996/12/08 15:15:44 niklas Exp $
#	$NetBSD: Makefile,v 1.13 1996/10/02 16:19:51 ws Exp $

LIB=	sa

DIR=${SAREL}${SADIR}

.PATH: ${DIR}

#DEBUGFLAGS= -DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG -DDEBUG -DPARANOID -Wall
#DEBUGFLAGS= -ansi -pedantic -Wall
AFLAGS+=${XAFLAGS}
CFLAGS+=${XCFLAGS} -D_STANDALONE -DCOMPAT_UFS $(DEBUGFLAGS) -I${DIR} -I${DIR}/../..
#
# Needed for PowerPC
CFLAGS+=$(EXTRACFLAGS)
CPPFLAGS+=${XCPPFLAGS}

.if ${MACHINE_ARCH}=="i386"
.PATH: ${DIR}/../../arch/${MACHINE_ARCH}/stand
.include "${DIR}/../../arch/${MACHINE_ARCH}/stand/Makefile.inc"
.endif

# stand routines
SRCS+=	alloc.c memcpy.c exit.c exec.c getfile.c gets.c globals.c \
	printf.c strerror.c strcmp.c memset.c memcmp.c strncpy.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c ioctl.c \
	lseek.c open.c nullfs.c read.c stat.c fstat.c write.c

.if !defined(NO_NET)
# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c
.endif

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	ufs.c nfs.c cd9660.c

NOPROFILE=
NOPIC=
OBJMACHINE=

install:

.include <bsd.lib.mk>
