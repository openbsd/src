#	$OpenBSD: Makefile,v 1.2 2000/03/03 00:54:49 todd Exp $

RELOC=0xfff10000

S=	${.CURDIR}/../../../..

SRTOBJ= crt0.o 
DIR_SA=$S/lib/libsa
DIR_KERN=$S/lib/libkern
M=$S/lib/libkern/arch/${MACHINE_ARCH}

.if exists ($M/Makefile.inc)
.PATH: $M
.include "$M/Makefile.inc"
.endif

SRC_net= nfs.c rpc.c net.c ether.c arp.c in_cksum.c netif.c \
	 bootparam.c rarp.c

#SRC_sa = alloc.c bcopy.c memcpy.c close.c getfile.c open.c \
#	 printf.c read.c strerror.c ufs.c globals.c lseek.c \
#	 closeall.c dev.c dkcksum.c nullfs.c fstat.c

SRC_sa = alloc.c memcpy.c memcmp.c memset.c strncmp.c close.c getfile.c \
	open.c printf.c read.c strerror.c ufs.c globals.c lseek.c \
	 dev.c

SRC_kern= ashrdi3.c bcmp.c bzero.c strcmp.c strlen.c strncpy.c __main.c \
	urem.S udiv.S sdiv.S umul.S rem.S

SRC_here= clock.c devopen.c dvma.c \
	 gets.c panic.c \
	 promboot.c zs.c exec_sun.c \
	 boot.c conf.c version.c dev_net.c if_le.c intr.c # promcons.c

SRCS= ${SRC_net} ${SRC_sa} ${SRC_kern} ${SRC_here}

OBJS=  ${SRTOBJ} ${SRCS:S/.c/.o/g:S/.S/.o/g}

DEFS= -DSTANDALONE -DSUN_BOOTPARAMS # -DNFS_DEBUG -DDEBUG
INCL=-I${.CURDIR} -I${S} -I${S}/lib/libsa -I${M}
COPTS= #-fno-defer-pop
CFLAGS= -g ${COPTS} ${DEFS} ${DBG} ${INCL}


.PATH:  ${DIR_SA} ${DIR_KERN} ${DIR_KERN_ARCH} 

all:	netboot

.S.o:
	${CC} -c ${CFLAGS} -o $@ $<

netboot: ${OBJS}
	${LD} -N -Ttext ${RELOC} -e start -o $@ ${OBJS}
	cp $@ /tftpboot

install:
	${INSTALL} ${INSTALL_COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	  netboot.bin ${DESTDIR}${MDEC_DIR}/netboot

.include <bsd.prog.mk>
