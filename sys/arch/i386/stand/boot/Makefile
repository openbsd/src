#	$OpenBSD: Makefile,v 1.18 1998/05/19 18:59:11 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c bootarg.c conf.c
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD?=	ld
LDFLAGS+=-nostdlib -Ttext $(LINKADDR) -z -x -Bstatic
SIZE?=	size
MAN=	boot.8
MLINKS=	boot.8 boot.conf.8
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

.PATH:	${S}/stand/boot

all:	machine-links

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@$(SIZE) $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CFLAGS+=$(SACFLAGS)
