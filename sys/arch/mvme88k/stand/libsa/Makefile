#	$OpenBSD: Makefile,v 1.13 2008/09/19 20:18:03 miod Exp $

LIB=sa

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..

SRCS=	board.c bugdev.c clock.c parse_args.c exec_mvme.c

.PATH:	${S}/lib/libsa
SRCS+=	alloc.c memcpy.c exit.c getfile.c gets.c globals.c \
	printf.c strerror.c memset.c memcmp.c strncpy.c strcmp.c strlen.c \
	strlcpy.c strlcat.c snprintf.c \
	close.c closeall.c dev.c dkcksum.c \
	lseek.c open.c nullfs.c read.c fstat.c \
	ufs.c cread.c
SRCS+=	arp.c ether.c in_cksum.c net.c netif.c rpc.c nfs.c \
	rarp.c bootparam.c
SRCS+=	loadfile.c

.PATH:	${S}/lib/libkern/arch/m88k ${S}/lib/libkern
SRCS+=	ashrdi3.c bzero.c __main.c

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
DEFS= -D__INTERNAL_LIBSA_CREAD -D_STANDALONE
#DEFS+=-DNETIF_DEBUG
INCL= -I${.CURDIR} -I${.CURDIR}/../libbug -I${S}/lib/libsa -I${S} -I.
CFLAGS+= ${XCFLAGS} ${COPTS} ${DEFS} ${DBG} ${INCL}

all: libsa.a

install:

.include <bsd.lib.mk>

.if !make(obj)
.BEGIN:
	@([ -h dev ] || ln -s ${.CURDIR}/../../../${MACHINE}/dev dev)
	@([ -h machine ] || ln -s ${.CURDIR}/../../../${MACHINE}/include machine)
	@([ -h m88k ] || ln -s ${.CURDIR}/../../../m88k/include m88k)
.NOPATH: dev machine m88k
CLEANFILES+=	dev machine m88k
.endif
