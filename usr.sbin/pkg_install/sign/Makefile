# define PGP to be the path to pgp (default: /usr/local/bin/pgp)
# and GZCAT to be the path to gzcat (default: /usr/bin/gzcat)

# Use the check_sign target if your system can't handle pipes or getpass
all: pkg_sign 

OBJS=main.o sign.o check.o gzip.o common.o
OBJS2=varmain.o check.o gzip.o common.o

pkg_sign:  $(OBJS)
	${CC} ${CFLAGS} -o $@ $(OBJS)

check_sign: $(OBJS2)
	${CC} ${CFLAGS} -o $@ $(OBJS2)

varmain.o: main.c
	${CC} ${CFLAGS} -DCHECKER_ONLY -o varmain.o -c main.c

clean:
	rm *.o
