#	$OpenBSD: Makefile,v 1.4 1998/09/29 07:14:52 mickey Exp $

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
diskless: ${.CURDIR}/../../compile/DISKLESS/bsd
	gzip -9c ${.CURDIR}/../../compile/DISKLESS/bsd > diskless
ADDBOOT+=diskless
.endif

boot.lif:	${PROG} ${ADDBOOT}
	${STRIP} ${PROG}
	${MKBOOT} -v ${PROG} ${ADDBOOT} boot.lif

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@${SIZE} $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=${DEBUGFLAGS} -DRELOC=$(LOADADDR)
CFLAGS+=$(SACFLAGS)
