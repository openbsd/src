#	$OpenBSD: Makefile.boot,v 1.3 1999/02/24 20:14:55 art Exp $
#
# a very simple makefile...
#
# You only want to use this if you aren't running OpenBSD.
#
CC=gcc -O
CFLAGS= -I. -DMAKE_BOOTSTRAP

# Uncomment this if your system does not have strtoul (i.e. SunOS)
STRTOUL= -Dstrtoul=strtol

# Note: The scanner here uses features specific to "flex" so
# do not bother even trying to make lex build the scanner.
# If you do not have flex, the source can be found in:
# src/usr.bin/lex (See Makefile.boot)
LEX=flex -l

YACC=yacc

OBJS=	files.o hash.o main.o mkheaders.o mkioconf.o mkmakefile.o \
	mkswap.o pack.o sem.o util.o y.tab.o lex.yy.o strerror.o

config: ${OBJS}
	${CC} -o $@ ${OBJS}

y.tab.o : y.tab.c
	${CC} ${CFLAGS} -c y.tab.c

y.tab.c y.tab.h : gram.y
	${YACC} -d gram.y

lex.yy.o : lex.yy.c
	${CC} ${CFLAGS} ${STRTOUL} -c lex.yy.c

lex.yy.c : scan.l
	${LEX} scan.l

${OBJS} : config.h

y.tab.o mkmakefile.o mkswap.o sem.o : sem.h
lex.yy.o : y.tab.h

.c.o:
	${CC} ${CFLAGS} -c $<

clean:
	rm -f *.o config lex.yy.c y.tab.[ch]

