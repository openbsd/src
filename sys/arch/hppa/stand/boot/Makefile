#	$OpenBSD: Makefile,v 1.5 1999/01/25 21:53:31 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c vars.c bootarg.c conf.c
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD?=	ld
LDFLAGS+=-Bstatic -nostartfiles -nostdlib -N -Ttext $(LINKADDR)
LDFLAGS+=-T ${.CURDIR}/ld.script
#LDFLAGS+=-O -N -S -H -R$(LINKADDR) -e begin -t
SIZE?=	size
MAN=	boot.8
MLINKS=	boot.8 boot.conf.8
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..
CLEANFILES+=	boot.lif bsd

LDADD=	${LIBSA} ${LIBZ} ${LIBKERN}
DPADD=	${LIBSA} ${LIBZ} ${LIBKERN}

.PATH:	${S}/stand/boot

all:	boot.lif

.if exists(${.CURDIR}/../../compile/DISKLESS/bsd)
bsd: ${.CURDIR}/../../compile/DISKLESS/bsd
	gzip -9c ${.CURDIR}/../../compile/DISKLESS/bsd > bsd
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

CPPFLAGS+=${DEBUGFLAGS} -DRELOC=$(LOADADDR)
CFLAGS+=$(SACFLAGS)
