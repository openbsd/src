#	$OpenBSD: Makefile,v 1.1 1996/10/30 22:40:51 niklas Exp $
#	$NetBSD: Makefile,v 1.3 1996/10/18 06:02:24 thorpej Exp $

.PATH: ${.CURDIR}/.. ${.CURDIR}/../../../../lib/libsa

BOOT_PROG = netboot
BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}

BOOT_SRCS = start.S netboot.c prom.c prom_disp.S OSFpal.c prom_swpal.S
BOOT_SRCS+= printf.c bzero.c rpcc.S

BOOT_SRCS+= dev_net.c conf.c devopen.c
BOOT_SRCS+= nfs.c rpc.c alloc.c ntohl.c htonl.c ntohs.c htons.c net.c bootp.c
BOOT_SRCS+= strlen.c bcopy.c dev.c bcmp.c strerror.c rarp.c read.c lseek.c
BOOT_SRCS+= in_cksum.c exit.c closeall.c arp.c strncpy.c globals.c open.c
BOOT_SRCS+= close.c ether.c netif.c

BOOT_SRCS+= if_prom.c loadfile.c getsecs.c

BOOT_OBJS = ${BOOT_SRCS:N*.h:R:S/$/.o/g}

HEADERSIZE_PROG = headersize

AFLAGS += -DASSEMBLER
CPPFLAGS += -I${.CURDIR}/../../../../ \
	    -I${.CURDIR}/../../../../lib/libsa \
	    -I${.CURDIR}/../.. -DPRIMARY_BOOTBLOCK \
	    -DALPHA_BOOT_ECOFF -DALPHA_BOOT_ELF
CFLAGS = -Werror -mno-fp-regs -g

CLEANFILES+= vers.c vers.o

.PATH: ${.CURDIR} ${.CURDIR}/../../../../lib/libkern ${.CURDIR}/../boot

all: ${BOOT_PROG}

${BOOT_PROG}: ${BOOT_OBJS} ${HEADERSIZE_PROG}
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version
	${COMPILE.c} vers.c
	${LD} -Ttext ${BOOT_RELOC} -N -e start -o ${BOOT_PROG}.hdr \
	    ${BOOT_OBJS} vers.o -lc # XXX
	size ${BOOT_PROG}.hdr
	strip ${BOOT_PROG}.hdr
	dd if=${BOOT_PROG}.hdr of=${BOOT_PROG} \
	    bs=`./${HEADERSIZE_PROG} ${BOOT_RELOC} ${BOOT_PROG}.hdr` skip=1

install:
	${INSTALL} -c -o bin -g bin -m 444 ${BOOT_PROG} \
	    ${DESTDIR}${BINDIR}/${BOOT_PROG}

clean: _SUBDIRUSE
	rm -f a.out [Ee]rrs mklog core *.core \
	    ${BOOT_PROG} ${BOOT_OBJS} ${CLEANFILES} \
	    ${BOOT_PROG}.hdr ${BOOT_PROG}.nohdr ${HEADERSIZE_PROG}

cleandir: _SUBDIRUSE clean

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
