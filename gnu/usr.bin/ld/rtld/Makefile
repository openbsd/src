#	$OpenBSD: Makefile,v 1.8 2002/12/12 09:54:46 markus Exp $
#	$NetBSD: Makefile,v 1.11 1995/10/09 00:11:24 pk Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c vfprintf.c
#NOMAN=	noman
MAN= ld.so.1
LDDIR?= $(.CURDIR)/..
LIBCRT0=
#PICFLAG=-pic
# As there is relocation going on behind GCC's back, don't cache function addresses.
PICFLAG=-fpic -fno-function-cse
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE_ARCH) $(PICFLAG) -DRTLD -DLIBC_SCCS
LDFLAGS+=-nostartfiles -nostdlib -Wl,-Bshareable,-Bsymbolic,-assert,nosymbolic
ASFLAGS+=-k
DPADD+= ${LIBC}
LDADD+=	-lc_pic
BINDIR= /usr/libexec

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH) ${.CURDIR}/../../../../lib/libc/stdio

.S.o:
	${CPP} ${.IMPSRC} | ${AS} ${ASFLAGS} -o ${.TARGET} -

.include <bsd.prog.mk>
