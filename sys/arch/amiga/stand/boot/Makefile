#	$OpenBSD: Makefile,v 1.6 2000/02/12 15:29:06 espie Exp $

NOPROG=	installboot
MAN=installboot.8
MANSUBDIR=/amiga

.if ${MACHINE}== "amiga"

SUBDIR=aout2bb txlt libsa

AOUT2BB!=cd $(.CURDIR)/aout2bb; \
	printf "xxx:\n\techo \$${.OBJDIR}/aout2bb\n" | \
	${MAKE} -r -s -f - xxx | grep aout2bb

TXLT!=	cd $(.CURDIR)/txlt; \
	printf "xxx:\n\techo \$${.OBJDIR}/txlt\n" | \
	${MAKE} -r -s -f - xxx | grep txlt

OBJS=	configure.o main.o
OBJS+=	console.o xd.o

# libsa library replacements:
OBJS+=	alloc.o printf.o twiddle.o

# libkern replacements:
OBJS+=	bcopy.o ashrdi3.o muldi3.o

# machine language startup code:
OBJS+=	startit.o

SRCS=	installboot.sh configure.c main.c console.c xd.c twiddle.c
SRCS+=	bbstart.s libstubs.s alloc.s printf.s startit.s
SRCS+=	bcopy.s ashrdi3.s muldi3.s

CLEANFILES=	$(OBJS) $(BOOTBLOCKS) x.out xxstart.s f.out fdstart.s

S=		${.CURDIR}/../../../..
INCPATH=	-I${.CURDIR} -I${.CURDIR}/../.. -I${S} -I${S}/lib/libsa
COPTIM=		-O -fomit-frame-pointer -fno-function-cse -Wa,-l -m68060 \
		-Wa,-m68030
CFLAGS=		${COPTIM} ${INCPATH} ${DEFS}
COPTS+=		-Wall -Wstrict-prototypes

BOOTBLOCKS=	xxboot # XXX  fdboot is too large at the moment.

.s.o:
	$(CC) $(CAFLAGS) $(COPTS) -x assembler-with-cpp -o $*.o -c $<

#libs:

.include "${.CURDIR}/libsa/Makefile.inc"
LIBSA=  ${SA_LIB}

# libstubs should really be a library, but we don't care for now.
LIBS=	${LIBSA} ${LIBKERN} libstubs.o

all: $(BOOTBLOCKS) installboot

$(OBJS): txlt

xxstart.s: bbstart.s
	cpp ${.CURDIR}/bbstart.s > $@
	
fdstart.s: bbstart.s
	cpp -DAUTOLOAD=8192 ${.CURDIR}/bbstart.s > $@

x.out: xxstart.o $(OBJS) $(LIBS)
	$(LD) $(LDFLAGS) -r -dc -e _start -o x.out xxstart.o $(OBJS) $(LIBS)
	size x.out

f.out: fdstart.o $(OBJS) $(LIBS)
	$(LD) $(LDFLAGS) -r -dc -e _start -o f.out fdstart.o $(OBJS) $(LIBS)
	size f.out


$(BOOTBLOCKS): aout2bb

xxboot: x.out
	rm -f $@
	${AOUT2BB} x.out $@ || nm -u x.out

fdboot: f.out
	rm -f $@
	${AOUT2BB} -F f.out $@ || nm -u f.out

clean::
	rm -f $(OBJS) fdstart.[os] xxstart.[os] f.out x.out installboot

install: all maninstall
	install -o $(BINOWN) -g $(BINGRP) $(BOOTBLOCKS) /usr/mdec
	install -o $(BINOWN) -g $(BINGRP) installboot /usr/sbin

ci:
	ci -l $(SRCS) Makefile

tar:
	(cd ..; tar \
		--exclude '*.o' --exclude RCS --exclude .depend \
		--exclude '*.out' --exclude fdstart.s --exclude xxstart.s \
		--exclude aout2bb/aout2bb --exclude libsa.a \
		-czvf boot.tar.gz boot)

test: xxtest fdtest

xxtest: xxboot
	dd if=$? of=/dev/rsd1e bs=8192 count=1

fdtest: fdboot
	dd if=$? of=/dev/rfd0a bs=8192 count=1

.endif

.include <bsd.prog.mk>
.include "Makefile.txlt"
