#	$OpenBSD: Makefile,v 1.3 1997/08/06 23:44:10 grr Exp $
#	$NetBSD: Makefile,v 1.6 1995/04/23 07:58:41 cgd Exp $

PROG=	grep
SRCS=	dfa.c grep.c getopt.c kwset.c obstack.c regex.c search.c
CFLAGS+=-DGREP -DHAVE_STRING_H=1 -DHAVE_SYS_PARAM_H=1 -DHAVE_UNISTD_H=1 \
	-DHAVE_GETPAGESIZE=1 -DHAVE_MEMCHR=1 -DHAVE_STRERROR=1 -DMULT=2

LINKS+= ${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep
MLINKS= grep.1 egrep.1 grep.1 fgrep.1

check:	all
	sh ${.CURDIR}/tests/check.sh ${.CURDIR}/tests

.include <bsd.prog.mk>
