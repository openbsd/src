#	$OpenBSD: Makefile,v 1.15 2015/10/22 18:54:41 miod Exp $
#	$NetBSD: Makefile,v 1.12 1997/04/17 07:27:49 thorpej Exp $

S=	${.CURDIR}/../../../..

.PATH: ${.CURDIR}/..

PROG = bootxx

SRCS = start.S bootxx.c prom.c prom_disp.S puts.c
NOMAN=
INSTALL_STRIP=
BINMODE= 444

BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}

CLEANFILES+= ${PROG}.sym ${PROG}.nosym ${PROG}.trunc

.PATH:	${S}/lib/libkern/arch/alpha ${S}/lib/libkern
SRCS+=	memmove.S bzero.c

DEFNS= -DPRIMARY_BOOTBLOCK

AFLAGS += -DASSEMBLER ${DEFNS} -fno-pie
CPPFLAGS += -I${.CURDIR}/../.. -I${S} ${DEFNS}
CFLAGS = -Werror -mno-fp-regs -fno-pie \
	-msmall-data -msmall-text

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym
	objcopy --output-target=binary ${PROG}.nosym ${PROG}.trunc
	dd if=${PROG}.trunc of=${PROG} obs=`expr 15 \* 512` conv=osync

# no lint here (yet?)
lint:

.include <bsd.prog.mk>

${PROG}.sym: ${OBJS}
	${LD} -nopie -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym ${OBJS}
	size ${PROG}.sym
