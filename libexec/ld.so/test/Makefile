#	$OpenBSD: Makefile,v 1.1.1.2 2000/06/13 03:40:29 rahnds Exp $

DIR=/usr/src/libexec/ld.so/obj/ld.so
.if (${MACHINE_ARCH} == "powerpc")
#necssary to build the shared objects. not necessary for dltest but
#doesn't hurt
CFLAGS += -fpic
.endif
CFLAGS += -g
LDFLAGS += -Wl,--export-dynamic -Wl,-dynamic-linker -Wl,${DIR}
#LDFLAGS += -Wl,--export-dynamic 

SRCS=	ldt.c
PROG=	ldt
MAN=
CLEANFILES= dltest dltest.o libfoo.so libfoo.o libbar.so libbar.o libdep.so
CLEANFILES+= libdep.o CCtest dltest A.o tst.o libA.so

all:	dltest CCtest

dltest: dltest.o libfoo.so libbar.so libdep.so
	$(CC) ${CFLAGS} -o $@ dltest.o ${LDFLAGS} -ldl

libfoo.so: libfoo.o
	$(LD) -x --shared --soname=libfoo.so -o libfoo.so libfoo.o

libbar.so: libbar.o 
	$(LD) -x --shared --soname=libbar.so -o libbar.so libbar.o 

libdep.so: libdep.o
	$(LD) -x --shared --soname=libdep.so -o libdep.so libdep.o

A.o:	A.C
	g++ -c ${CFLAGS} ${.IMPSRC}

libA.so:	A.o
	$(LD) -x --shared -soname $@ -o $@ /usr/lib/crtbeginS.o A.o /usr/lib/crtendS.o 

CCtest:	libA.so tst.o
	g++ ${LDFLAGS} -o $@ tst.o -L . -lA

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
