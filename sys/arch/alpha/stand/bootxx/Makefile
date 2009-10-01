#	$OpenBSD: Makefile,v 1.10 2009/10/01 15:42:14 miod Exp $
#	$NetBSD: Makefile,v 1.12 1997/04/17 07:27:49 thorpej Exp $

S=	${.CURDIR}/../../../..

.PATH: ${.CURDIR}/..

PROG = bootxx

SRCS = start.S bootxx.c prom.c prom_disp.S puts.c
NOMAN=
INSTALL_STRIP=
BINMODE= 444

BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}
HEADERSIZE_PROG = headersize

CLEANFILES+= ${PROG}.sym ${PROG}.nosym ${PROG}.trunc ${HEADERSIZE_PROG}

.PATH:	${S}/lib/libkern/arch/alpha ${S}/lib/libkern
SRCS+=	bzero.c memcpy.c

DEFNS= -DPRIMARY_BOOTBLOCK
DEFNS+= -DALPHA_BOOT_ELF

AFLAGS += -DASSEMBLER ${DEFNS}
CPPFLAGS += -I${.CURDIR}/../.. -I${S} ${DEFNS}
CFLAGS = -Werror -mno-fp-regs

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym #${HEADERSIZE_PROG}
	objcopy --output-target=binary ${PROG}.nosym ${PROG}.trunc
	dd if=${PROG}.trunc of=${PROG} obs=`expr 15 \* 512` conv=osync
#	dd if=${PROG}.nosym of=${PROG} \
#	    ibs=`./${HEADERSIZE_PROG} ${BOOT_RELOC} ${PROG}.nosym` skip=1 \
#	    obs=`expr 15 \* 512` conv=osync

# no lint here (yet?)
lint:

.include <bsd.prog.mk>

${PROG}.sym: ${OBJS}
	${LD} -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym ${OBJS}
	size ${PROG}.sym
