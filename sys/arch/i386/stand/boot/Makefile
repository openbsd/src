#	$OpenBSD: Makefile,v 1.3 1997/03/31 23:06:20 mickey Exp $

PROG=	boot
SRCS=	srt0.S boot.c cmd.c conf.c
CFLAGS+=$(SACFLAGS) -I${.CURDIR}/../../../../lib/libsa -I${.CURDIR}/../libsa
AFLAGS+=-Wa,-R
# AFLAGS+=-Wa,-a
LD=ld
LDFLAGS+=-Ttext $(START) -z -x -e start_boot -Bstatic
MAN=	boot.8

LDADD=	-L../libsa -lsa -L../libz -lz
DPADD=	../libsa/libsa.a ../libz/libz.a

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
