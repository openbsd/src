#	$OpenBSD: Makefile,v 1.4 2000/03/09 00:08:07 mickey Exp $
#	$NetBSD: Makefile,v 1.6 1995/04/23 07:58:41 cgd Exp $

PROG=	grep
SRCS=	dfa.c grep.c getopt.c getopt1.c kwset.c obstack.c regex.c savedir.c \
	search.c stpcpy.c
CFLAGS+=-I${.CURDIR} -DVERSION=\"2.4.1\" -DPACKAGE=\"grep\" -DGREP \
	-DSTDC_HEADERS  -DHAVE_MEMCHR -DHAVE_MEMMOVE -DHAVE_MEMCPY \
	-DHAVE_DIRENT_H -DHAVE_UNISTD_H -DHAVE_STRERROR \
	-DHAVE_SETMODE -DHAVE_GETPAGESIZE -DHAVE_MMAP

LINKS+= ${BINDIR}/grep ${BINDIR}/egrep \
	${BINDIR}/grep ${BINDIR}/fgrep
MLINKS= grep.1 egrep.1 grep.1 fgrep.1


check:	all
	GREP=./grep sh ${.CURDIR}/tests/bre.sh ${.CURDIR}/tests
	#GREP=./grep sh ${.CURDIR}/tests/ere.sh ${.CURDIR}/tests
	GREP=./grep sh ${.CURDIR}/tests/khadafy.sh ${.CURDIR}/tests
	GREP=./grep sh ${.CURDIR}/tests/options.sh ${.CURDIR}/tests
	GREP=./grep sh ${.CURDIR}/tests/spencer1.sh ${.CURDIR}/tests
	GREP=./grep sh ${.CURDIR}/tests/status.sh ${.CURDIR}/tests
	GREP=./grep sh ${.CURDIR}/tests/warning.sh ${.CURDIR}/tests

.include <bsd.prog.mk>
