#	$OpenBSD: Makefile,v 1.9 2003/12/09 12:42:41 jmc Exp $
#
# By default, flex will be configured to generate 8-bit scanners only if the
# -8 flag is given.  If you want it to always generate 8-bit scanners, add
# "-DDEFAULT_CSIZE=256" to CFLAGS.  Note that doing so will double the size
# of all uncompressed scanners.
# 
# If on your system you have trouble building flex due to 8-bit character
# problems, remove the -8 from FLEX_FLAGS and the "#define FLEX_8_BIT_CHARS"
# from the beginning of flexdef.h.
#
# To bootstrap lex, cp initscan.c to scan.c and run make.

PROG=	lex
CFLAGS+=-I. -I${.CURDIR}
SRCS=	ccl.c dfa.c ecs.c gen.c main.c misc.c nfa.c parse.c sym.c tblcmp.c \
	yylex.c
OBJS+=	scan.o skel.o
CLEANFILES+=parse.c parse.h scan.c skel.c y.tab.c y.tab.h
LDADD=	-ll
DPADD=	${LIBL}

MAN = flex.1

LINKS=	${BINDIR}/lex ${BINDIR}/flex \
	${BINDIR}/lex ${BINDIR}/flex++
MLINKS=	flex.1 lex.1 flex.1 flex++.1

# This just gets installed verbatim
.if make(install)
SUBDIR+= PSD.doc
.endif

parse.h parse.c: parse.y
	${YACC} -d ${.ALLSRC}
	mv y.tab.c parse.c
	mv y.tab.h parse.h

skel.c:	flex.skl mkskel.sh
	sh ${.CURDIR}/mkskel.sh ${.CURDIR}/flex.skl > skel.c

scan.c: scan.l
	flex -t -p ${.CURDIR}/scan.l > scan.c

scan.o: parse.c

includes:
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/FlexLexer.h ${DESTDIR}/usr/include/g++

.include <bsd.prog.mk>
