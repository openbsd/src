#	$OpenBSD: Makefile,v 1.18 2014/02/23 19:22:40 miod Exp $

LIB=	sa

CPPFLAGS+=	${SA_CPPFLAGS}

CFLAGS+=-I${.CURDIR} -I${.CURDIR}/../include -I${.CURDIR}/../.. \
	-I${.CURDIR}/../../.. -I${.CURDIR}/../../../.. \
	-I${.CURDIR}/../../../../lib/libsa

# local source
SRCS+=	devopen.c exec_hp300.c rawfs.c

# stand routines
.PATH:	${.CURDIR}/../../../../lib/libsa
SRCS+=	alloc.c exit.c getfile.c gets.c globals.c loadfile.c \
	memcmp.c memcpy.c memset.c printf.c snprintf.c strcmp.c \
	strerror.c strlcat.c strlen.c strncmp.c strncpy.c

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
SRCS+=	ufs.c nfs.c cd9660.c

NOPROFILE=
NOPIC=

install:

.include <bsd.lib.mk>
