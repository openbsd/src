#	$OpenBSD: Makefile,v 1.1.1.1 2000/06/13 03:34:17 rahnds Exp $

CFLAGS += -g
LDFLAGS += -Wl,--export-dynamic -Wl,-dynamic-linker -Wl,/home/staff/pefo/newldso/ld.so/ld.so

SRCS=	ldt.c
PROG=	ldt
MAN=

all:	dltest

dltest: dltest.o libfoo.so libbar.so libdep.so
	$(CC) -o $@ dltest.o ${LDFLAGS} -ldl

libfoo.so: libfoo.o
	$(LD) -x --shared --soname=libfoo.so -o libfoo.so libfoo.o

libbar.so: libbar.o
	$(LD) -x --shared --soname=libbar.so -o libbar.so libbar.o libdep.so

libdep.so: libdep.o
	$(LD) -x --shared --soname=libdep.so -o libdep.so libdep.o

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
