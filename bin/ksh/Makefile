#	$OpenBSD: Makefile,v 1.1.1.1 1996/08/14 06:19:12 downsj Exp $

PROG=	ksh
SRCS=	alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c \
	eval.c exec.c expr.c history.c io.c jobs.c lex.c mail.c \
	main.c misc.c missing.c path.c shf.c sigact.c syn.c table.c trap.c \
	tree.c tty.c var.c version.c vi.c

DEFS=	-DHAVE_CONFIG_H
CFLAGS+=${DEFS} -I. -I${.CURDIR}

CLEANFILES+=	siglist.out emacs.out

.depend trap.o: siglist.out
.depend emacs.o: emacs.out

siglist.out: config.h sh.h siglist.in siglist.sh
	${.CURDIR}/siglist.sh "${CPP} ${CPPFLAGS} ${DEFS} -I${.CURDIR}" \
		< ${.CURDIR}/siglist.in > siglist.out

emacs.out:
	${.CURDIR}/emacs-gen.sh ${.CURDIR}/emacs.c > emacs.out

.include <bsd.prog.mk>
