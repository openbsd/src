all:	boot boot.out
CFLAGS+=-fwritable-strings -I${.CURDIR}/../include
CFLAGS+=-I${.CURDIR}/../.. -I${.CURDIR}/../../machine
CFLAGS+=-I/usr/src/sys
LDFLAGS+= -L ${.CURDIR}/../libbug -L/usr/local/lib
BOOT=FC0000
#BOOT=1000000

LIBBUG!= cd $(.CURDIR)/../libbug; \
     printf "xxx:\n\techo \$${.OBJDIR}/libbug.a\n" | ${MAKE} -r -s -f - xxx

LDADD+=${LIBBUG} #/usr/local/lib/libgcc.a
SRCS+=bugcrt.c bugio.c main.c

.PATH:    ${.CURDIR}/../bugcrt ${.CURDIR}/../libbug ${.CURDIR}/../../../../lib/libc_sa ${.CURDIR}/${MACHINE_ARCH}

boot:	bugcrt.o main.o bcopy.o memset.o printf.o ${LIBBUG}
#	ld -o {.TARGET} -x -n -Ttext ${BOOT} bugcrt.o bugio.o main.o bcopy.o memset.o printf.o /usr/local/lib/libgcc.a
	ld -o ${.TARGET} -x -N -Ttext ${BOOT} ${.ALLSRC} ${LDADD}

boot.out:
	${.CURDIR}/wrtvid ${.OBJDIR}/boot && mv ${.OBJDIR}/boot.? ${.CURDIR}

#main.o: main.c
#	${CC} ${CFLAGS} -c -O ${.ALLSRC}
#	${LD} -x -r ${.TARGET}
#	${LD} -x  ${.TARGET}
#	mv a.out ${.TARGET}

.include <bsd.prog.mk>
