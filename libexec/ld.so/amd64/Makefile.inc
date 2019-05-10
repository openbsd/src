#	$OpenBSD: Makefile.inc,v 1.5 2019/05/10 13:29:21 guenther Exp $

CFLAGS += -fPIC -mno-sse2 -mno-sse -mno-3dnow -mno-mmx
AFLAGS += -fpic
LD_SCRIPT = ${.CURDIR}/${MACHINE_CPU}/ld.script
