#	$OpenBSD: Makefile,v 1.15 1997/09/29 23:01:06 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c conf.c
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD=ld
LDFLAGS+=-nostdlib -Ttext $(LINKADDR) -z -x -Bstatic
MAN=	boot.8
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
	@size $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CFLAGS+=$(SACFLAGS)
