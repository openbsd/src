#	$OpenBSD: Makefile,v 1.1 1999/08/16 09:43:10 downsj Exp $

LIB=	saz

.PATH:	${.CURDIR}/../../../../lib/libsa ${.CURDIR}/../libsa

CFLAGS= -O2 -msoft-float -D__INTERNAL_LIBSA_CREAD \
	-I${.CURDIR} -I${.CURDIR}/../include -I${.CURDIR}/../.. \
	-I${.CURDIR}/../../.. -I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../lib/libsa

# stand routines
SRCS=	alloc.c devopen.c exit.c exec.c getfile.c gets.c globals.c \
	memcmp.c memcpy.c memset.c printf.c strerror.c strncpy.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c cread.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c rawfs.c cd9660.c

${OBJS}:	${.CURDIR}/../Makefile.inc

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
