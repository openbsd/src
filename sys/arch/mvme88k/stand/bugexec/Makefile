all:	hello
SRCS= hello.c
OBJS= hello.o

CFLAGS+=-I${.CURDIR}/include -I${.CURDIR}/${MACHINE_ARCH}
CFLAGS+=-I${.CURDIR}/../include -I${.CURDIR}/../.. -I/usr/src/sys
CFLAGS+=-fwritable-strings

LIBBUG!= cd $(.CURDIR)/../libbug; \
     printf "xxx:\n\techo \$${.OBJDIR}/libbug.a\n" | ${MAKE} -r -s -f - xxx

BUGCRT!= cd $(.CURDIR)/../bugcrt; \
     printf "xxx:\n\techo \$${.OBJDIR}/bugcrt.o\n" | ${MAKE} -r -s -f - xxx

KERNCRT!= cd $(.CURDIR)/../kerncrt; \
     printf "xxx:\n\techo \$${.OBJDIR}/kerncrt.o\n" | ${MAKE} -r -s -f - xxx

LDADD+=${LIBBUG} /usr/local/lib/libgcc.a

hello:	$(OBJS) ${LIBBUG}
	${LD} -x -Ttext 10020 ${KERNCRT} $(OBJS) ${LDADD} -o ${.TARGET}
clean:
	rm -f a.out *.core
	rm -f hello.o hello.bug hello.bug.1 hello.bug.2

.include <bsd.prog.mk>
