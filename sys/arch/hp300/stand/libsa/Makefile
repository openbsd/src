#	$OpenBSD: Makefile,v 1.15 2007/12/28 18:56:08 miod Exp $

LIB=	sa

.include "${.CURDIR}/../Makefile.inc"

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../include -I${.CURDIR}/../.. \
	-I${.CURDIR}/../../.. -I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../lib/libsa

# stand routines
.PATH:	${.CURDIR}/../../../../lib/libsa
SRCS+=	alloc.c devopen.c exit.c exec.c getfile.c gets.c globals.c \
	memcmp.c memcpy.c memset.c printf.c snprintf.c strcmp.c \
	strerror.c strlen.c strncmp.c strncpy.c

.PATH:	${.CURDIR}/../../../../lib/libkern
SRCS+=	__main.c

# io routines
SRCS+=	close.c closeall.c dev.c disklabel.c dkcksum.c fstat.c ioctl.c lseek.c \
	open.c read.c stat.c write.c

# network routines
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c

# network info services
SRCS+=	bootp.c bootparam.c rarp.c

# boot filesystems
SRCS+=	ufs.c nfs.c rawfs.c cd9660.c

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
