### ko stuff
OS_VER	!= uname -r

SRCS		+= koerror.c kocell.c ports.c kodebug.c
SRCS		+= sysname.c
CLEANFILES	+= sysname.c gensysname
CFLAGS		+= -I${.CURDIR}/../src/lib/ko

.PATH: ${.CURDIR}/../src/lib/ko

sysname.c: gensysname
	./gensysname -c > sysname.c

gensysname: gensysname.c
	${CC} ${CFLAGS} -o gensysname ${.CURDIR}/../src/lib/ko/gensysname.c ../libroken/libroken.a
