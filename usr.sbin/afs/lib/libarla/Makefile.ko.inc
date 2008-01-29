### ko stuff

.PATH:	$(ROKENSRC)

OS_VER	!= uname -r

SRCS	+= koerror.c \
	   kocell.c \
	   ports.c \
	   vlmisc.c \
	   part.c \
	   agetarg.c \
	   uae.c \
	   sysname.c \
	   afs_uuid.c \
	   misc.c \
	   ko_resolve.c

# resolve.c exists in roken as well *sigh*
ko_resolve.c:
	ln -sf $(AFSSRC)/lib/ko/resolve.c ko_resolve.c

CLEANFILES	+= sysname.c gensysname gensysname.o getarg.o strupr.o \
		   get_window_size.o
CFLAGS		+= -I${AFSSRC}/lib/ko

.PATH: ${AFSSRC}/lib/ko
.PATH: ${.CURDIR}/${AFSTOPDIR}/../../kerberosV/src/lib/roken

sysname.c: gensysname
	./gensysname -c > sysname.c

gensysname.o: vldb.h lwp.h
gensysname: gensysname.o getarg.o strupr.o get_window_size.o
	${HOSTCC} ${CFLAGS} -o gensysname gensysname.o getarg.o strupr.o get_window_size.o

CLEANFILES+=	gensysname.o agetarg.o
