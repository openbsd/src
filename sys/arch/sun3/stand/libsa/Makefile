#	$NetBSD: Makefile,v 1.9 1995/11/17 23:23:52 gwr Exp $

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

SRC_sun3= control.c ctrlsp.S

SRC_here= clock.c dev_disk.c devopen.c dvma.c \
	 exec_sun.c gets.c netif_sun.c panic.c \
	 promboot.c promcons.c promdev.c

SRCS= ${SRC_net} ${SRC_sa} ${SRC_kern} ${SRC_sun3} ${SRC_here}

CLEANFILES+=	SRT0.o SRT1.o

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

DEFS= -DSTANDALONE -DCOMPAT_UFS
INCL= -I${.CURDIR} -I${S}/lib/libsa -I${S}
COPTS= -msoft-float
CFLAGS= -O ${COPTS} ${DEFS} ${DBG} ${INCL}

.PATH:  ${DIR_SA} ${DIR_KERN} ${.CURDIR}/../../sun3

all: libsa.a SRT0.o SRT1.o

install:

.include <bsd.lib.mk>
