#	$OpenBSD: Makefile,v 1.5 1997/05/05 06:01:53 millert Exp $
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

CLEANFILES+= ${PROG}.sym ${PROG}.nosym ${HEADERSIZE_PROG}

DEFNS= -DPRIMARY_BOOTBLOCK

AFLAGS += -DASSEMBLER ${DEFNS}
CPPFLAGS += -I${.CURDIR}/../.. -I${S} ${DEFNS}
CFLAGS = -Werror -mno-fp-regs

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym ${HEADERSIZE_PROG}
	dd if=${PROG}.nosym of=${PROG} \
	    ibs=`./${HEADERSIZE_PROG} ${BOOT_RELOC} ${PROG}.nosym` skip=1 \
	    obs=`expr 15 \* 512` conv=osync

# no lint here (yet?)
lint:

.include <bsd.prog.mk>

### find out what to use for libkern
KERN_AS=	library
KERNDST=	${.CURDIR}/../libkern/${__objdir}
.include "${.CURDIR}/../libkern/Makefile.inc"
LIBKERN=	${KERNLIB}

${PROG}.sym: ${OBJS} ${LIBKERN}
	${LD} -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym ${OBJS} \
	    ${LIBKERN}
	size ${PROG}.sym
