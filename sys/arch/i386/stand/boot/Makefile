#	$OpenBSD: Makefile,v 1.30 2003/04/17 03:43:18 drahn Exp $

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

SRCS+=	boot_loadfile.c cmd.c vars.c bootarg.c conf.c
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

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

