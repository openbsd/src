#	$OpenBSD: Makefile,v 1.5 1996/10/30 22:40:38 niklas Exp $
#	$NetBSD: Makefile,v 1.10 1996/10/18 06:01:41 thorpej Exp $

.PATH: ${.CURDIR}/.. ${.CURDIR}/../../../../lib/libsa

BOOT_PROG = boot
BOOT_RELOC = ${SECONDARY_LOAD_ADDRESS}

BOOT_SRCS = start.S boot.c loadfile.c disk.c conf.c prom.c prom_disp.S OSFpal.c

BOOT_SRCS+= alloc.c bzero.c close.c dev.c devopen.c disklabel.c dkcksum.c
BOOT_SRCS+= getfile.c gets.c ioctl.c lseek.c open.c printf.c read.c
BOOT_SRCS+= strcmp.c ufs.c write.c bcopy.c filesystem.c strlen.c
BOOT_SRCS+= ntohl.c prom_swpal.S
BOOT_OBJS = ${BOOT_SRCS:N*.h:R:S/$/.o/g}

HEADERSIZE_PROG = headersize

DEFNS= -DCOMPAT_UFS -DALPHA_BOOT_ECOFF -DALPHA_BOOT_ELF

AFLAGS += -DASSEMBLER ${DEFNS}
CPPFLAGS += -I${.CURDIR}/../.. -I${.CURDIR}/../../../..
CFLAGS = -Werror -mno-fp-regs -g ${DEFNS}

CLEANFILES+= vers.c vers.o

.PATH: ${.CURDIR}/../../../../lib/libkern

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
	    ${BOOT_PROG}.hdr ${HEADERSIZE_PROG}

cleandir: _SUBDIRUSE clean

.include "${.CURDIR}/../Makefile.inc"
.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.subdir.mk>
