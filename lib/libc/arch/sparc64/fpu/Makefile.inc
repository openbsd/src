#	$OpenBSD: Makefile.inc,v 1.1 2003/07/21 18:41:30 jason Exp $
SRCS += fpu_add.c fpu_compare.c fpu_div.c fpu_explode.c fpu_implode.c \
	fpu_mul.c fpu_qp.c fpu_q.c fpu_sqrt.c fpu_subr.c fpu_reg.c
.PATH: ${.CURDIR}/arch/sparc64/fpu
