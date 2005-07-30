#	$OpenBSD: Makefile,v 1.32 2005/07/30 14:32:46 millert Exp $

.include "${.CURDIR}/../Makefile.inc"

MAN=	boot.8
MLINKS=	boot.8 boot.conf.8

.if ${MACHINE} == "i386"
PROG=	boot
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic

.if defined(DEBUGFLAGS) && !empty(DEBUGFLAGS:M-D_TEST)
CLEANFILES+=	srt0.o
SRCS=	crt0.c
.else
LDFLAGS+=-Ttext $(LINKADDR) -N -x -noinhibit-exec
CLEANFILES+=	crt0.o
SRCS=	srt0.S
.endif

SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ} ${LIBKERN}
DPADD=	${LIBSA} ${LIBZ} ${LIBKERN}

.PATH:	${S}/stand/boot

boot.bin: boot
	objcopy -v -O binary ${PROG} boot.bin

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o ${PROG} $(OBJS) $(LDADD)
	@$(SIZE) ${PROG}

.else
NOPROG=
.endif

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CPPFLAGS+=-DLINKADDR=${LINKADDR}
CFLAGS+=$(SACFLAGS)
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a

