#	$OpenBSD: Makefile,v 1.15 2015/11/20 18:57:18 tedu Exp $
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

PROG=   lex
CFLAGS+=-I. -I${.CURDIR} -DHAVE_CONFIG_H
SRCS= buf.c ccl.c dfa.c ecs.c filter.c gen.c main.c misc.c \
	  nfa.c options.c parse.y regex.c scan.l scanflags.c \
	  scanopt.c skel.c sym.c tables.c tables_shared.c \
	  tblcmp.c yylex.c

CLEANFILES+=skel.c parse.c parse.h
LDADD+=-lm

MAN = flex.1

LINKS=  ${BINDIR}/lex ${BINDIR}/flex \
		${BINDIR}/lex ${BINDIR}/flex++
MLINKS= flex.1 lex.1 flex.1 flex++.1

VERSION="2.5.39"

parse.h parse.c: parse.y
	yacc -d -o parse.c ${.CURDIR}/parse.y

skel.c: flex.skl mkskel.sh flexint.h tables_shared.h
	sed -e 's/m4_/m4postproc_/g' -e 's/m4preproc_/m4_/g' \
		${.CURDIR}/flex.skl | m4 -I${.CURDIR} -P \
		-DFLEX_MAJOR_VERSION=`echo ${VERSION} | cut -f 1 -d .` \
		-DFLEX_MINOR_VERSION=`echo ${VERSION} | cut -f 2 -d .` \
		-DFLEX_SUBMINOR_VERSION=`echo ${VERSION} | cut -f 3 -d .` | \
		sed -e 's/m4postproc_/m4_/g' | \
		sh ${.CURDIR}/mkskel.sh  > ${.TARGET}

includes:
.if !exists(${DESTDIR}/usr/include/g++)
	${INSTALL} -d -o root -g bin -m 755 \
		${DESTDIR}/usr/include/g++
.endif
	${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${.CURDIR}/FlexLexer.h ${DESTDIR}/usr/include/g++/

.include <bsd.prog.mk>
