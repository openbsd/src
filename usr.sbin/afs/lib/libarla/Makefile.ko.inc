### ko stuff
OS_VER	!= uname -r

SRCS		+= koerror.c kocell.c ports.c vlmisc.c part.c agetarg.c
SRCS		+= sysname.c
CLEANFILES	+= sysname.c gensysname gensysname.o
CFLAGS		+= -I${AFSSRC}/lib/ko

.PATH: ${AFSSRC}/lib/ko
.PATH: ${.CURDIR}/${AFSTOPDIR}/../../kerberosV/src/lib/roken

sysname.c: gensysname
	./gensysname -c > sysname.c

gensysname: gensysname.o agetarg.o
	${HOSTCC} ${CFLAGS} -o gensysname gensysname.o agetarg.o ${LIBROKEN}

CLEANFILES+=	gensysname.o agetarg.o

