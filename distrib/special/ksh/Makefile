#	$OpenBSD: Makefile,v 1.1 2014/12/23 17:16:02 deraadt Exp $

PROG=	ksh
SRCS=	alloc.c c_ksh.c c_sh.c c_test.c c_ulimit.c edit.c emacs.c eval.c \
	exec.c expr.c history.c io.c jobs.c lex.c mail.c main.c mknod.c \
	misc.c path.c shf.c syn.c table.c trap.c tree.c tty.c var.c \
	version.c vi.c

DEFS=	-Wall
CFLAGS+=${DEFS} -I. -I${.CURDIR}/../../../bin/ksh -I${.CURDIR}/../../../lib/libc/gen
MAN=	ksh.1 sh.1

LINKS=	${BINDIR}/ksh ${BINDIR}/rksh
LINKS+=	${BINDIR}/ksh ${BINDIR}/sh
MLINKS=	ksh.1 rksh.1

.PATH:  ${.CURDIR}/../../../bin/ksh
.include <bsd.prog.mk>
