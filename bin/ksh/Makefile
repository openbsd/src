#	$OpenBSD: Makefile,v 1.20 2004/12/18 21:04:52 millert Exp $

PROG=	ksh
SRCS=	alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c \
	eval.c exec.c expr.c history.c io.c jobs.c lex.c mail.c \
	main.c misc.c path.c shf.c syn.c table.c trap.c \
	tree.c tty.c var.c version.c vi.c

DEFS=	-Wall -Wno-unused
CFLAGS+=${DEFS} -I. -I${.CURDIR}
MAN=	ksh.1 sh.1

CLEANFILES+=	siglist.out emacs.out

LINKS=	${BINDIR}/ksh ${BINDIR}/rksh
LINKS+=	${BINDIR}/ksh ${BINDIR}/sh
MLINKS=	ksh.1 rksh.1 ksh.1 ulimit.1

.depend trap.o: siglist.out
.depend emacs.o: emacs.out

siglist.out: config.h sh.h siglist.in siglist.sh
	/bin/sh ${.CURDIR}/siglist.sh \
		"${CC} -E ${CPPFLAGS} ${DEFS} -I${.CURDIR}" \
		< ${.CURDIR}/siglist.in > siglist.out

emacs.out: emacs.c
	/bin/sh ${.CURDIR}/emacs-gen.sh ${.CURDIR}/emacs.c > emacs.out

check test:
	/bin/sh ${.CURDIR}/tests/th.sh ${.CURDIR}/tests/th -s ${.CURDIR}/tests -p ./ksh -C pdksh,sh,ksh,posix,posix-upu

.include <bsd.prog.mk>
