#	$OpenBSD: Makefile,v 1.3 2000/10/13 05:15:17 drahn Exp $

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
CLEANFILES+= libdep.o CCtest dltest A.o B.o tst.o libA.so libB.so

all:	dltest CCtest

dltest: dltest.o libfoo.so libbar.so libdep.so
	$(CC) ${CFLAGS} -o $@ dltest.o ${LDFLAGS} -ldl

libfoo.so: libfoo.o
	$(CC) -shared -o libfoo.so libfoo.o

libbar.so: libbar.o 
	$(CC) -shared -o libbar.so libbar.o 

libdep.so: libdep.o
	$(CC) -shared -o libdep.so libdep.o

A.o:	A.C
	g++ -c ${CFLAGS} ${.IMPSRC}

libA.so:	A.o
	$(CC) -shared -o $@ A.o 

B.o:	B.C
	g++ -c ${CFLAGS} ${.IMPSRC}

libB.so:	B.o
	$(CC) -shared -o $@ B.o 

CCtest:	libA.so libB.so tst.o
	g++ ${LDFLAGS} -o $@ tst.o -L . -lB -lA

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
