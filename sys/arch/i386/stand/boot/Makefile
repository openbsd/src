#	$OpenBSD: Makefile,v 1.12 1997/09/02 21:18:12 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c conf.c
#AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD=ld
LDFLAGS+=-nostdlib -Ttext $(LINKADDR) -z -x -Bstatic
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

kentry.o: kentry.c
	@echo ${COMPILE.c} ${.IMPSRC}
	@${COMPILE.c} -S -o tmp.s ${.IMPSRC}
	@sed 's/ret/lret/g' tmp.s | ${AS} -f -o $@
	@rm tmp.s

${PROG}: $(OBJS) $(DPADD)
	$(LD) $(LDFLAGS) -o $(PROG) $(OBJS) $(LDADD)
	@size $(PROG)

.include <bsd.prog.mk>

CPPFLAGS+=-DBOOTMAGIC=$(BOOTMAGIC) ${DEBUGFLAGS}
CFLAGS+=$(SACFLAGS)
