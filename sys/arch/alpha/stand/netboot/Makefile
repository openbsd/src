#	$OpenBSD: Makefile,v 1.3 1997/05/05 06:01:59 millert Exp $
#	$NetBSD: Makefile,v 1.11 1997/04/17 07:27:50 thorpej Exp $

S=	${.CURDIR}/../../../..

.PATH: ${.CURDIR}/.. ${.CURDIR}/../boot

PROG = netboot

SRCS = start.S boot.c prom.c prom_disp.S OSFpal.c prom_swpal.S
SRCS+= rpcc.S
SRCS+= dev_net.c conf.c devopen.c
SRCS+= if_prom.c loadfile.c getsecs.c
NOMAN=
INSTALL_STRIP=
BINMODE= 444

BOOT_RELOC = ${PRIMARY_LOAD_ADDRESS}
HEADERSIZE_PROG = headersize

CLEANFILES+= vers.c vers.o ${PROG}.sym ${PROG}.nosym ${HEADERSIZE_PROG}

DEFNS= -DPRIMARY_BOOTBLOCK -DALPHA_BOOT_ECOFF # -DALPHA_BOOT_ELF

AFLAGS += -DASSEMBLER ${DEFNS}
CPPFLAGS += -I${.CURDIR}/../.. -I${S} ${DEFNS}
CFLAGS = -Werror -mno-fp-regs

${PROG}.nosym: ${PROG}.sym
	cp ${PROG}.sym ${PROG}.nosym
	strip ${PROG}.nosym

${PROG}: ${PROG}.nosym ${HEADERSIZE_PROG}
	dd if=${PROG}.nosym of=${PROG} \
	    bs=`./${HEADERSIZE_PROG} ${BOOT_RELOC} ${PROG}.nosym` skip=1

# no lint here (yet?)
lint:

.include <bsd.prog.mk>

### find out what to use for libkern
KERN_AS=	library
KERNDST=	${.CURDIR}/../libkern/${__objdir}
.include "${.CURDIR}/../libkern/Makefile.inc"
LIBKERN=	${KERNLIB}

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

${PROG}.sym: ${OBJS} ${LIBSA} ${LIBZ} ${LIBKERN}
	sh ${.CURDIR}/newvers.sh ${.CURDIR}/version
	${COMPILE.c} vers.c
	${LD} -Ttext ${BOOT_RELOC} -N -e start -o ${PROG}.sym \
	    ${OBJS} vers.o ${LIBSA} ${LIBZ} ${LIBSA} ${LIBKERN}
	size ${PROG}.sym
