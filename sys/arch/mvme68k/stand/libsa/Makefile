#	$OpenBSD: Makefile,v 1.21 2008/03/11 20:07:53 deraadt Exp $

LIB=sa

CLEANFILES+=SRT0.o

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..

SRCS+=	bugdev.c clock.c parse_args.c exec_mvme.c

.PATH:  $S/lib/libsa
SRCS+=	alloc.c memcpy.c exit.c getfile.c gets.c globals.c printf.c \
	strerror.c memset.c memcmp.c strncpy.c strcmp.c strlen.c \
	snprintf.c close.c closeall.c dev.c dkcksum.c lseek.c open.c \
	nullfs.c read.c fstat.c ufs.c cread.c
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c nfs.c rarp.c bootparam.c

.PATH:	${S}/lib/libkern
SRCS+=	ashrdi3.c muldi3.c

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
DEFS= -D__INTERNAL_LIBSA_CREAD
INCL= -I${.CURDIR} -I${.CURDIR}/../libbug -I${S}/lib/libsa -I${S}
CFLAGS+= ${XCFLAGS} -O2 ${COPTS} ${DEFS} ${DBG} ${INCL}

all: libsa.a SRT0.o

install:

.include <bsd.lib.mk>
