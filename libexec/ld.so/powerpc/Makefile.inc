#	$OpenBSD: Makefile.inc,v 1.3 2019/05/10 13:29:21 guenther Exp $

CFLAGS += -fpic -msoft-float
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
