#	$OpenBSD: Makefile,v 1.16 2007/11/25 18:25:29 deraadt Exp $
#	$NetBSD: Makefile,v 1.17 1997/04/17 07:27:46 thorpej Exp $

S=	${.CURDIR}/../../../..

.PATH: ${.CURDIR}/..

PROG = boot

SRCS = start.S boot.c loadfile.c disk.c conf.c prom.c prom_disp.S OSFpal.c
SRCS+= devopen.c filesystem.c prom_swpal.S
NOMAN=
INSTALL_STRIP=
BINMODE= 444

BOOT_RELOC = ${SECONDARY_LOAD_ADDRESS}
HEADERSIZE_PROG = headersize

CLEANFILES+= vers.c vers.o ${PROG}.sym ${PROG}.nosym ${HEADERSIZE_PROG}

.PATH:	${S}/lib/libkern/arch/alpha ${S}/lib/libkern
SRCS+=	__divlu.S __remqu.S __reml.S strlen.c bzero.c __divl.S __divqu.S \
	__remlu.S

DEFNS= -DCOMPAT_UFS -DALPHA_BOOT_ELF

AFLAGS += -DASSEMBLER ${DEFNS}
CPPFLAGS += -I${.CURDIR}/../../../../lib/libsa -I${.CURDIR}/../.. -I${S} ${DEFNS}
CFLAGS = -Os -Werror -mno-fp-regs

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym #${HEADERSIZE_PROG}
	objcopy --output-target=binary ${PROG}.nosym ${PROG}
#	dd if=${PROG}.nosym of=${PROG} \
#	    bs=`./${HEADERSIZE_PROG} ${BOOT_RELOC} ${PROG}.nosym` skip=1

# no lint here (yet?)
lint:

.include <bsd.prog.mk>

### find out what to use for libz
Z_AS=		library
ZDST=		${.CURDIR}/../libz/${__objdir}
.include "${.CURDIR}/../libz/Makefile.inc"
LIBZ=		${ZLIB}

### find out what to use for libsa
SA_AS=		library
SADST=		${.CURDIR}/../libsa/${__objdir}
SAREL=
SA_ZLIB=	yes
.include "${.CURDIR}/../libsa/Makefile.inc"
LIBSA=		${SALIB}

${PROG}.sym: ${OBJS} ${LIBSA} ${LIBZ}
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version
	${COMPILE.c} vers.c
	${LD} -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym \
	    ${OBJS} vers.o ${LIBSA} ${LIBZ} ${LIBSA}
	size ${PROG}.sym
