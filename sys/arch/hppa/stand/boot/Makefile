#	$OpenBSD: Makefile,v 1.1.1.1 1998/06/23 18:46:41 mickey Exp $

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

LDADD=	${LIBSA} ${LIBZ} ${LIBKERN}
DPADD=	${LIBSA} ${LIBZ} ${LIBKERN}

.PATH:	${S}/stand/boot

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@${SIZE} $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=${DEBUGFLAGS} -DRELOC=$(LOADADDR)
CFLAGS+=$(SACFLAGS)
