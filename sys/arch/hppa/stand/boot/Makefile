#	$OpenBSD: Makefile,v 1.12 2002/12/18 01:20:41 deraadt Exp $

MAN=	boot.8
MANSUBDIR=/hppa
MLINKS=	boot.8 boot.conf.8
S	=${.CURDIR}/../../../..
CLEANFILES+=	boot.gdb boot.map boot.lif bsd

.if ${MACHINE} == "hppa"
PROG=	boot
SRCS=	srt0.S exec.c boot.c cmd.c vars.c bootarg.c conf.c
LD?=	ld
LDFLAGS+=-Bstatic -nostartfiles -nostdlib -N -Ttext $(LINKADDR)
LDFLAGS+=-T ${.CURDIR}/ld.script -Map boot.map
SIZE?=	size
STRIP?=	strip
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ} ${LIBKERN}
DPADD=	${LIBSA} ${LIBZ} ${LIBKERN}

.PATH:	${S}/stand/boot

all:	boot.lif

#.if exists(${.CURDIR}/../../compile/DISKLESS/bsd)
#bsd: ${.CURDIR}/../../compile/DISKLESS/bsd
#	gzip -9 -c ${.CURDIR}/../../compile/DISKLESS/bsd > bsd
#ADDBOOT+=bsd
#.endif

.if exists(${.CURDIR}/../../compile/GENERIC/bsd)
bsd: ${.CURDIR}/../../compile/GENERIC_PIE/bsd
	gzip -9 -c ${.CURDIR}/../../compile/GENERIC/bsd > bsd
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
.else
NOPROG=
.endif

.include <bsd.prog.mk>

CPPFLAGS+=${DEBUGFLAGS} -DRELOC=${LOADADDR} -DHEAP_LIMIT=${HEAP_LIMIT}
CFLAGS+=$(SACFLAGS)
