#	$Id: Makefile,v 1.7 1995/12/26 17:46:28 deraadt Exp $

LIB=sa

CLEANFILES+=SRT0.o SRT1.o

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa
DIR_KERN=$S/lib/libkern

SRC_net= nfs.c rpc.c net.c ether.c arp.c in_cksum.c netif.c \
	 bootparam.c rarp.c


SRC_sa = alloc.c bcopy.c memcpy.c close.c getfile.c open.c \
	 printf.c read.c strerror.c ufs.c globals.c lseek.c \
	 closeall.c dev.c dkcksum.c nullfs.c fstat.c

SRC_kern= ashrdi3.c bcmp.c bzero.c strcmp.c strlen.c

SRC_sun3= exec_sun.c

SRC_here= clock.c devopen.c dvma.c \
	 gets.c panic.c \
	 promboot.c promcons.c

SRCS= ${SRC_net} ${SRC_sa} ${SRC_kern} ${SRC_sun3} ${SRC_here}

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
INCL= -I. -I${S}/lib/libsa -I${S}
COPTS= #-fno-defer-pop
CFLAGS= -O2 ${COPTS} ${DEFS} ${DBG} ${INCL}

.PATH:  ${DIR_SA} ${DIR_KERN} ../../sun3

all: libsa.a SRT0.o SRT1.o

install:

.include <bsd.lib.mk>
