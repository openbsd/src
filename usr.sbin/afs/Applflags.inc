LDADD	+= -lkrb -ldes ../libarla/libarla.a -ltermcap ../libroken/libroken.a -lreadline
DPADD	+= ../libroken/libroken.a ../libarla/libarla.a ${LIBKRB} ${LIBDES} ${LIBREADLINE} ${LIBTERMCAP}
CFLAGS	+= -I${.CURDIR}/../src/util -I${.CURDIR}/../src/lib/roken \
	   -I${.CURDIR}/../src/include -I${.CURDIR}/../libroken \
	   -I${.CURDIR}/../src/rxdef -I${.CURDIR}/../src/lwp -I../libarla \
	   -I${.CURDIR}/../src/arlad -I${.CURDIR}/../src \
	   -I${.CURDIR}/../src/lib/ko -I${.CURDIR}/../src/rxkad \
	   -I${.CURDIR}/../src/lib/sl
	   
