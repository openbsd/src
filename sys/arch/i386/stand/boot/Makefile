#	$OpenBSD: Makefile,v 1.7 1997/06/30 21:11:19 kstailey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c conf.c
CFLAGS+=$(SACFLAGS) -I${.CURDIR}/../../../../lib/libsa -I${.CURDIR}/../libsa
AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD=ld
LDFLAGS+=-nostdlib -Ttext $(START) -z -x -e start_boot -Bstatic
INSTALL_STRIP=
MAN=	boot.8
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
.ifdef NO_NET
CPPFLAGS+=-DNO_NET
.endif
