#	$NetBSD: Makefile,v 1.1.1.1 1995/07/25 23:12:21 chuck Exp $

LIB=sa

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa
DIR_KERN=$S/lib/libkern

SRC_net= nfs.c rpc.c net.c ether.c arp.c in_cksum.c \
	 bootparam.c rarp.c

SRC_sa = alloc.c bcopy.c close.c getfile.c open.c \
	 printf.c read.c strerror.c ufs.c 

SRC_kern= ashrdi3.c bcmp.c bzero.c strcmp.c strlen.c

SRC_here= clock.c devopen.c \
	 exec.c gets.c netif.c panic.c \
	 machdep.c

SRCS= ${SRC_net} ${SRC_sa} ${SRC_kern} ${SRC_here}

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

DEFS= -DCOMPAT_UFS
INCL= -I. -I${S}/lib/libsa -I${S}
COPTS= -msoft-float -fno-defer-pop -fno-omit-frame-pointer
CFLAGS= -O ${COPTS} ${DEFS} ${DBG} ${INCL}

.PATH:  ${DIR_SA} ${DIR_KERN} ../../sun3

all: libsa.a SRT0.o SRT1.o

install:

.include <bsd.lib.mk>
