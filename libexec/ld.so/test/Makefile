#	$OpenBSD: Makefile,v 1.9 2002/02/11 13:58:49 art Exp $

DIR=/usr/src/libexec/ld.so/obj/ld.so
#necessary to build the shared objects. not necessary for dltest but
#doesn't hurt
CFLAGS += -fpic
CFLAGS += -g
CFLAGS += -I${.CURDIR}/.. -I${.CURDIR}/../${MACHINE_ARCH}
LDFLAGS += -Wl,--export-dynamic -Wl,-dynamic-linker,${DIR} -Wl,-rpath,.
#LDFLAGS += -Wl,--export-dynamic 

SRCS=	ldt.c
PROG=	ldt
MAN=
CLEANFILES= dltest dltest.o libfoo.so libfoo.o libbar.so libbar.o libdep.so
CLEANFILES+= libdep.o CCtest dltest A.o B.o tst.o libA.so libB.so

all:	dltest CCtest

dltest: dltest.o libfoo.so libbar.so libdep.so
	$(CC) ${CFLAGS} -o $@ dltest.o ${LDFLAGS}

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
	g++ ${LDFLAGS} -o $@ tst.o libB.so libA.so

run:
	@echo running CCtest
	LD_LIBRARY_PATH=. CCtest
	@echo running dltest -l bar -f bar
	LD_LIBRARY_PATH=. dltest -l bar -f bar
	@echo running dltest -l foo -f foo
	LD_LIBRARY_PATH=. dltest -l foo -f foo

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
