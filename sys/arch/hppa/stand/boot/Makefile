#	$OpenBSD: Makefile,v 1.9 1999/12/23 02:42:23 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c vars.c bootarg.c conf.c
LD?=	ld
LDFLAGS+=-Bstatic -nostartfiles -nostdlib -N -Ttext $(LINKADDR)
LDFLAGS+=-T ${.CURDIR}/ld.script -Map boot.map
SIZE?=	size
MAN=	boot.8
MANSUBDIR=/hppa
MLINKS=	boot.8 boot.conf.8
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..
CLEANFILES+=	boot.gdb boot.map boot.lif bsd

LDADD=	${LIBSA} ${LIBZ} ${LIBKERN}
DPADD=	${LIBSA} ${LIBZ} ${LIBKERN}

.PATH:	${S}/stand/boot

all:	boot.lif

# enable this when SHEPHERD dies
#.if exists(${.CURDIR}/../../compile/DISKLESS/bsd)
#bsd: ${.CURDIR}/../../compile/DISKLESS/bsd
#	gzip -9c ${.CURDIR}/../../compile/DISKLESS/bsd > bsd
#ADDBOOT+=bsd
#.endif

.if exists(${.CURDIR}/../../compile/SHEPHERD_PIE/bsd)
bsd: ${.CURDIR}/../../compile/SHEPHERD_PIE/bsd
	gzip -9c ${.CURDIR}/../../compile/SHEPHERD_PIE/bsd > bsd
ADDBOOT+=bsd
.endif

# probably we should check for 256k limit for ISL
boot.lif:	${PROG} ${ADDBOOT}
	-@cp ${PROG} ${PROG}.gdb
	${STRIP} ${PROG}
	${MKBOOT} -v ${PROG} ${ADDBOOT} boot.lif

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@${SIZE} $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=${DEBUGFLAGS} -DRELOC=${LOADADDR} -DHEAP_LIMIT=${HEAP_LIMIT}
CFLAGS+=$(SACFLAGS)
