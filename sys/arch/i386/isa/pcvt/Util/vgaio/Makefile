PROG=	vgaio
MAN8=	vgaio.${MAN8EXT}

SRCS=	vgaio.y lex.l
YACC=	yacc

#YFLAGS+= -yd	# Bison
#YFLAGS+= -v	# verbose

LFLAGS+= -I
LDADD=	-lm -ly -ll

CFLAGS+= -I ${.CURDIR}/obj -I ${.CURDIR} -g

CLEANFILES+= y.tab.h
CLEANFILES+= y.output	# comment file from bison

.include <bsd.prog.mk>
