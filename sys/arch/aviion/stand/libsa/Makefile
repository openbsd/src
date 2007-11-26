#	$OpenBSD: Makefile,v 1.2 2007/11/26 10:01:42 deraadt Exp $

LIB=sa

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa

SRC_net= arp.c ether.c in_cksum.c net.c netif.c rpc.c nfs.c \
	 rarp.c bootparam.c 

SRC_sa=	alloc.c memcpy.c exit.c getfile.c gets.c globals.c \
	printf.c strerror.c memset.c memcmp.c strncpy.c strcmp.c strlen.c \
	strlcpy.c strlcat.c snprintf.c \
	close.c closeall.c dev.c dkcksum.c \
	lseek.c open.c nullfs.c read.c fstat.c \
	ufs.c cread.c

.PATH:	${S}/lib/libkern/arch/m88k ${S}/lib/libkern
SRCS+=	ashrdi3.c bzero.c

SRC_aviion= exec_aout.c

SRC_here= clock.c parse_args.c

SRCS+= ${SRC_net} ${SRC_sa} ${SRC_aviion} ${SRC_here}

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
DEFS= -D__INTERNAL_LIBSA_CREAD 
#-DNETIF_DEBUG
INCL= -I${.CURDIR} -I${.CURDIR}/../libprom -I${S}/lib/libsa -I${S}
CFLAGS+= ${XCFLAGS} ${COPTS} ${DEFS} ${DBG} ${INCL}

.PATH:  ${DIR_SA} ${DIR_KERN} 

all: libsa.a

install:

.include <bsd.lib.mk>
