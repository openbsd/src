#	$OpenBSD: Makefile,v 1.6 2014/02/23 20:01:04 miod Exp $
#	$NetBSD: Makefile,v 1.9 2013/01/22 15:48:40 tsutsui Exp $
#	@(#)Makefile	8.2 (Berkeley) 8/15/93

NOMAN= # defined

S= ${.CURDIR}/../../../..

CPPFLAGS+=	-nostdinc -D_STANDALONE
CPPFLAGS+=	-I${.CURDIR} -I${.OBJDIR} -I${S} -I${S}/arch

CPPFLAGS+=	-DSUPPORT_DISK
CPPFLAGS+=	-DSUPPORT_ETHERNET
CPPFLAGS+=	-DSUPPORT_BOOTP
#CPPFLAGS+=	-DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG
#CPPFLAGS+=	-DRPC_DEBUG -DRARP_DEBUG -DNET_DEBUG -DDEBUG -DPARANOID

OBJCOPY?=	objcopy
TEXTADDR=	0x700000	# 7MB
LDSCRIPT=	${.CURDIR}/boot.ldscript
LINKFORMAT=	-static -N -Ttext ${TEXTADDR} -T ${LDSCRIPT} \
		--warn-common -nopie

SRCS=	locore.S
SRCS+=	init_main.c
SRCS+=	fault.c setjmp.S
SRCS+=	bcd.c
SRCS+=	devopen.c
SRCS+=	conf.c
SRCS+=	getline.c parse.c
SRCS+=	boot.c
SRCS+=	cons.c prf.c awaitkey.c
SRCS+=	sio.c
SRCS+=	bmc.c bmd.c font.c kbd.c
SRCS+=	sc.c sd.c
SRCS+=	ufs_disksubr.c

# netboot support
SRCS+=	if_le.c lance.c getsecs.c
SRCS+=	dev_net.c

PROG=   boot

### find out what to use for libkern
.PATH:	${S}/lib/libkern
SRCS+=	memcpy.c strlcat.c strlcpy.c strlen.c
SRCS+=	muldi3.c negdi2.c ashrdi3.c

### find out what to use for libsa
.PATH:	${S}/lib/libsa
SRCS+=	strtol.c

SAREL=
SADST=	.
USE_LOADFILE=	yes
.include "${S}/lib/libsa/Makefile.inc"

LIBS=	${SALIB} ${ZLIB} ${KERNLIB}

${PROG}: ${LDSCRIPT} ${OBJS} ${LIBS}
	${LD} ${LINKFORMAT} -x -o ${PROG}.elf ${OBJS} ${LIBS}
	${OBJCOPY} -S -O a.out-m88k-openbsd --impure ${PROG}.elf ${PROG}

install: ${PROG}
	install -m 555 -g bin -o bin ${PROG} ${DESTDIR}${MDEC_DIR}

CLEANFILES+=	${PROG}.elf

.BEGIN:
	@([ -h machine ] || ln -s ${.CURDIR}/../../include machine)
	@([ -h m88k ] || ln -s ${.CURDIR}/../../../m88k/include m88k)

.include <bsd.prog.mk>
