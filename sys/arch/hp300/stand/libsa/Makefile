#	$OpenBSD: Makefile,v 1.9 1997/07/13 07:21:55 downsj Exp $
#	$NetBSD: Makefile,v 1.5 1996/06/26 17:44:42 thorpej Exp $

LIB=	sa

.PATH:	${.CURDIR}/../../../../lib/libsa
.PATH:	${.CURDIR}/..

# Don't need these now...
# DEBUGFLAGS=-DNETIF_DEBUG -DRPC_DEBUG -DNFS_DEBUG -DRARP_DEBUG -DNET_DEBUG
# DEBUGFLAGS+=-DEXEC_DEBUG

CFLAGS+=-Dhp300 -DSTANDALONE -DCOMPAT_UFS -DNO_LSEEK
CFLAGS+=-D__INTERNAL_LIBSA_CREAD -DHEAP_LIMIT="0xffffdfff"
CFLAGS+=${DEBUGFLAGS} -I${.CURDIR}/..
CFLAGS+=-I${.CURDIR}/../.. -I${.CURDIR}/../../..
CFLAGS+=-I${.CURDIR}/../../../..  -I${.CURDIR}/../../../../lib/libsa

# stand routines
SRCS=	alloc.c exit.c exec.c getfile.c gets.c globals.c \
	memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c rawfs.o

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
