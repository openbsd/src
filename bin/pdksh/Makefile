#	$OpenBSD: Makefile,v 1.6 1996/08/20 16:50:30 downsj Exp $

PROG=	ksh
SRCS=	alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c \
	eval.c exec.c expr.c history.c io.c jobs.c lex.c mail.c \
	main.c misc.c missing.c path.c shf.c sigact.c syn.c table.c trap.c \
	tree.c tty.c var.c version.c vi.c

DEFS=	-DHAVE_CONFIG_H
CFLAGS+=${DEFS} -I. -I${.CURDIR}

CLEANFILES+=	siglist.out emacs.out

LINKS=	${BINDIR}/ksh ${BINDIR}/rksh
MLINKS=	ksh.1 rksh.1

.depend trap.o: siglist.out
.depend emacs.o: emacs.out

siglist.out: config.h sh.h siglist.in siglist.sh
	/bin/sh ${.CURDIR}/siglist.sh \
		"${CPP} ${CPPFLAGS} ${DEFS} -I${.CURDIR}" \
		< ${.CURDIR}/siglist.in > siglist.out

emacs.out: emacs.c
	/bin/sh ${.CURDIR}/emacs-gen.sh ${.CURDIR}/emacs.c > emacs.out

.include <bsd.prog.mk>
