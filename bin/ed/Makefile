#	$OpenBSD: Makefile,v 1.7 2003/07/26 20:22:13 jmc Exp $

PROG=	ed
CFLAGS+=-DBACKWARDS -DDES
SRCS=	 buf.c cbc.c glbl.c io.c main.c re.c sub.c undo.c

#LINKS=  ${BINDIR}/ed ${BINDIR}/red
#MLINKS= ed.1 red.1

# These just get installed verbatim
.if make(install)
SUBDIR+= USD.doc/09.edtut
.endif

.include <bsd.prog.mk>
