#	$Id: Makefile,v 1.2 2003/06/22 22:20:07 deraadt Exp $

PROG=	grep
SRCS=	binary.c file.c grep.c mmfile.c queue.c util.c
LINKS=  ${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep \
	${BINDIR}/grep ${BINDIR}/zgrep
MLINKS= grep.1 egrep.1 \
	grep.1 fgrep.1 \
	grep.1 zgrep.1

CFLAGS+= -I/usr/local/include -Wall -pedantic

LDADD=  -lz -L/usr/local/lib/ -liberty

.include <bsd.prog.mk>
