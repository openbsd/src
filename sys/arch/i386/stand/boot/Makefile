#	$OpenBSD: Makefile,v 1.9 1997/08/02 22:23:07 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c kentry.c conf.c
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD=ld
LDFLAGS+=-nostdlib -Ttext $(START) -z -x -Bstatic
INSTALL_STRIP=
MAN=	boot.8
S	=${.CURDIR}/../../../..
SADIR=	${.CURDIR}/..

LDADD=	${LIBSA} ${LIBZ}
DPADD=	${LIBSA} ${LIBZ}

all:	machine-links

machine-links:
	@rm -f machine i386
	@ln -fs ${.CURDIR}/../.. i386
	@ln -fs ${.CURDIR}/../../include machine

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC)
CFLAGS+=$(SACFLAGS)
.ifdef NO_NET
CPPFLAGS+=-DNO_NET
.endif
