#	$OpenBSD: Makefile,v 1.1 1996/09/18 08:15:23 pefo Exp $

SUBDIR= # libdl

CFLAGS += -I. -DNO_UNDERSCORE -DVERBOSE_DLINKER \
	-DUSE_CACHE -D__PIC__ -I${MACHINE_ARCH}

SRCS=	boot1.c hash.c readelflib1.c vsprintf.c elfinterp.c
PROG=	ld.so
MAN=	ld.so.8
BINDIR=/usr/libexec

.PATH:	${MACHINE_ARCH}

ELF_LDFLAGS=--shared # using GNU ld

$(PROG):
	$(LD) -x -e _dl_boot $(ELF_LDFLAGS) -o $(DLINKER) $(OBJS)


.include <bsd.prog.mk>
.include <bsd.subdir.mk>
