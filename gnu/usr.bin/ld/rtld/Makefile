#	$NetBSD: Makefile,v 1.11 1995/10/09 00:11:24 pk Exp $

PROG=	ld.so
SRCS=	mdprologue.S rtld.c malloc.c shlib.c etc.c md.c vfprintf.c
#NOMAN=	noman
MAN= rtld.1
LDDIR?= $(.CURDIR)/..
#PICFLAG=-pic
PICFLAG=-fpic
CFLAGS+=-I$(LDDIR) -I$(.CURDIR) -I$(LDDIR)/$(MACHINE_ARCH) $(PICFLAG) -DRTLD -DLIBC_SCCS
LDFLAGS+=-Bshareable -Bsymbolic -assert nosymbolic
ASFLAGS+=-k
LDADD+=	-lc_pic
BINDIR= /usr/libexec
MLINKS=	rtld.1 ld.so.1

.PATH: $(LDDIR) $(LDDIR)/$(MACHINE_ARCH) ${.CURDIR}/../../../../lib/libc/stdio

$(PROG):
	$(LD) -o $(PROG) $(LDFLAGS) $(OBJS) $(LDADD)

.S.o:
	${CPP} ${.IMPSRC} | ${AS} ${ASFLAGS} -o ${.TARGET} -

.include <bsd.prog.mk>
