#	$OpenBSD: Makefile,v 1.10 1996/05/16 02:30:38 chuck Exp $

LIB=sa

CLEANFILES+=SRT0.o 

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa
DIR_KERN=$S/lib/libkern

SRC_net= nfs.c rpc.c net.c ether.c arp.c in_cksum.c netif.c \
	 bootparam.c rarp.c


SRC_sa = alloc.c bcopy.c memcpy.c close.c exit.c getfile.c gets.c open.c \
	 printf.c read.c strerror.c ufs.c globals.c lseek.c \
	 closeall.c dev.c dkcksum.c nullfs.c fstat.c

SRC_kern= ashrdi3.c bcmp.c bzero.c strcmp.c strlen.c

SRC_mvme= exec_mvme.c

SRC_here= clock.c bugdev.c parse_args.c

SRCS= ${SRC_net} ${SRC_sa} ${SRC_kern} ${SRC_mvme} ${SRC_here}

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
INCL= -I. -I${.CURDIR}/../libbug -I${S}/lib/libsa -I${S}
COPTS= #-fno-defer-pop
CFLAGS= -O2 ${COPTS} ${DEFS} ${DBG} ${INCL}

.PATH:  ${DIR_SA} ${DIR_KERN} 

all: libsa.a SRT0.o 

install:

.include <bsd.lib.mk>
