#	$OpenBSD: Makefile,v 1.9 2005/09/28 23:54:03 deraadt Exp $
#	$NetBSD: Makefile,v 1.11 1995/10/09 00:11:24 pk Exp $

.include <bsd.own.mk>

TARGET_MACHINE_ARCH?=	$(MACHINE_ARCH)

.if !defined(NOPIC) && ($(TARGET_MACHINE_ARCH) == $(MACHINE_ARCH))
PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c vfprintf.c
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

.else
NOPROG=
.endif

MAN= ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH) ${.CURDIR}/../../../../lib/libc/stdio

.S.o:
	${CPP} ${.IMPSRC} | ${AS} ${ASFLAGS} -o ${.TARGET} -

.include <bsd.prog.mk>
