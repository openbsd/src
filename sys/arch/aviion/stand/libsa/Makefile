#	$OpenBSD: Makefile,v 1.6 2014/11/19 20:01:35 miod Exp $

LIB=sa

NOPIC=nopic
NOPROFILE=noprofile
NOPIE=

# Logically src/sys
S=${.CURDIR}/../../../..

SRCS=	clock.c delay.S exec.c fault.c parse_args.c setjmp.S

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c memcpy.c exit.c getfile.c gets.c globals.c loadfile.c \
	printf.c strerror.c memset.c memcmp.c strncpy.c strcmp.c strlen.c \
	strlcpy.c strlcat.c snprintf.c strchr.c strtol.c \
	close.c closeall.c dev.c dkcksum.c \
	lseek.c open.c nullfs.c read.c fstat.c disklabel.c \
	ufs.c cread.c
SRCS+= arp.c ether.c in_cksum.c net.c netif.c netudp.c rpc.c nfs.c \
	rarp.c bootparam.c

.PATH:	${S}/lib/libkern/arch/m88k ${S}/lib/libkern
SRCS+=	ashrdi3.c bzero.c __main.c

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
DEFS= -D__INTERNAL_LIBSA_CREAD -D_STANDALONE
#-DNETIF_DEBUG
INCL= -I${.CURDIR} -I${.CURDIR}/../libprom -I${S}/lib/libsa -I${S}
CFLAGS+= ${XCFLAGS} ${COPTS} ${DEFS} ${DBG} ${INCL}

all: libsa.a

install:

.include <bsd.lib.mk>
