#	$OpenBSD: Makefile,v 1.21 1998/05/28 20:52:36 mickey Exp $

.include "${.CURDIR}/../Makefile.inc"

PROG=	boot
LD?=	ld
SIZE?=	size
LDFLAGS+=-nostdlib -Bstatic

.if defined(DEBUGFLAGS) && !empty(DEBUGFLAGS:M-D_TEST)
CLEANFILES+=	srt0.o
SRCS=	crt0.c
.else
LDFLAGS+=-Ttext $(LINKADDR) -z -x
CLEANFILES+=	crt0.o
SRCS=	srt0.S
.endif

SRCS+=	boot.c cmd.c vars.c bootarg.c conf.c
MAN=	boot.8
MLINKS=	boot.8 boot.conf.8
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

.PATH:	${S}/stand/boot

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@$(SIZE) $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CFLAGS+=$(SACFLAGS)
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
